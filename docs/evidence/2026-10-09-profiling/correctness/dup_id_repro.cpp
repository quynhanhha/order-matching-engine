// Phase 3 correctness probe (not a test in the repo): behavior when a caller violates the
// README's "callers supply unique order IDs" precondition. No engine code is modified.
#include "order_book.h"
#include <cstdio>
#include <vector>

static int failures = 0;
#define EXPECT(cond, msg) do { bool ok_ = (cond); std::printf("  [%s] %s\n", ok_ ? "observed" : "NOT observed", msg); if (!ok_) ++failures; } while (0)

int main() {
    std::vector<Trade> trades;
    auto cb = [&trades](const Trade& t) { trades.push_back(t); };

    std::puts("S1: duplicate resting ID -> second order is not indexed (orphan, uncancellable)");
    {
        OrderBook book(16, cb);
        book.addLimitOrder(Side::Sell, 101, 1, 7, 1);   // A, indexed
        book.addLimitOrder(Side::Sell, 100, 1, 7, 1);   // B, same ID: rests, try_emplace drops it
        EXPECT(book.bestAsk() && book.bestAsk()->price == 100, "B (dup) rests at 100 with no error/return signal");
        book.cancelOrder(7);                            // removes A only
        EXPECT(book.bestAsk() && book.bestAsk()->price == 100, "cancel(7) removed A; B still rests");
        book.cancelOrder(7);                            // no-op: B unreachable by ID
        EXPECT(book.bestAsk() && book.bestAsk()->price == 100 && book.bestAsk()->head->orderId == 7,
               "second cancel(7) is a silent no-op: B can never be cancelled");
    }

    std::puts("S2: filling the orphan erases the OTHER order's index entry");
    {
        trades.clear();
        OrderBook book(16, cb);
        book.addLimitOrder(Side::Sell, 101, 1, 7, 1);   // A, indexed
        book.addLimitOrder(Side::Sell, 100, 1, 7, 1);   // B, orphan, better price
        book.addLimitOrder(Side::Buy, 100, 1, 9, 2);    // fills B -> orderIndex_.erase(7) removes A's entry
        EXPECT(trades.size() == 1 && trades[0].sellOrderId == 7 && trades[0].price == 100, "incoming buy filled B at 100");
        book.cancelOrder(7);                            // A's entry is gone -> no-op
        EXPECT(book.bestAsk() && book.bestAsk()->price == 101,
               "cancel(7) is a no-op: live order A at 101 lost its index entry and is uncancellable");
    }

    std::puts("S3: duplicate of an active ID on the opposite side");
    {
        trades.clear();
        OrderBook book(16, cb);
        book.addLimitOrder(Side::Buy, 90, 1, 7, 1);     // A bid, indexed
        book.addLimitOrder(Side::Sell, 110, 1, 7, 2);   // B ask, same ID, orphan
        book.cancelOrder(7);
        EXPECT(!book.bestBid() && book.bestAsk() && book.bestAsk()->price == 110,
               "cancel(7) removes the bid; the ask with the same ID remains, unreachable");
    }
    std::printf("probe complete; %d expectation(s) not observed\n", failures);
    return 0;
}
