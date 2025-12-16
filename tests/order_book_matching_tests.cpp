#include <gtest/gtest.h>
#include <vector>

#include "order_book.h"

// ─────────────────────────────────────────────────────────────────────────────
// TEST FIXTURE
// ─────────────────────────────────────────────────────────────────────────────

class OrderBookMatchingTest : public ::testing::Test {
protected:
    std::vector<Trade> trades_;

    void SetUp() override {
        trades_.clear();
    }

    TradeCallback captureCallback() {
        return [this](const Trade& t) { trades_.push_back(t); };
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 1. NO MATCHING (orders rest on book)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, BuyOrderRestsWhenNoAsks) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);

    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 50);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, SellOrderRestsWhenNoBids) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);

    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 50);
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, BuyOrderRestsWhenPriceBelowBestAsk) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);  // ask @ 100
    book.addLimitOrder(Side::Buy, 99, 50, 2, 200);    // buy @ 99, no cross

    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 99);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
}

TEST_F(OrderBookMatchingTest, SellOrderRestsWhenPriceAboveBestBid) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);   // bid @ 100
    book.addLimitOrder(Side::Sell, 101, 50, 2, 200);  // sell @ 101, no cross

    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 101);
}

// ─────────────────────────────────────────────────────────────────────────────
// 2. EXACT FILL (incoming fully fills, resting fully fills)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, BuyExactlyFillsSell) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);
    book.addLimitOrder(Side::Buy, 100, 50, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 2);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 50);

    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, SellExactlyFillsBuy) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);
    book.addLimitOrder(Side::Sell, 100, 50, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 2);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 50);

    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3. PARTIAL FILL - INCOMING REMAINDER RESTS
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, BuyPartiallyFillsRemainderRests) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 30, 1, 100);  // resting 30
    book.addLimitOrder(Side::Buy, 100, 50, 2, 200);   // incoming 50

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 2);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 30);

    EXPECT_EQ(book.bestAsk(), nullptr);  // resting fully filled
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 20);  // 50 - 30 remains
}

TEST_F(OrderBookMatchingTest, SellPartiallyFillsRemainderRests) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 100, 30, 1, 100);   // resting 30
    book.addLimitOrder(Side::Sell, 100, 50, 2, 200);  // incoming 50

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 2);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 30);

    EXPECT_EQ(book.bestBid(), nullptr);  // resting fully filled
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 20);  // 50 - 30 remains
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. PARTIAL FILL - RESTING REMAINDER STAYS
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, BuyPartiallyFillsRestingRemains) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);  // resting 50
    book.addLimitOrder(Side::Buy, 100, 30, 2, 200);   // incoming 30

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 2);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 30);

    EXPECT_EQ(book.bestBid(), nullptr);  // incoming fully filled
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 20);  // 50 - 30 remains
}

TEST_F(OrderBookMatchingTest, SellPartiallyFillsRestingRemains) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);   // resting 50
    book.addLimitOrder(Side::Sell, 100, 30, 2, 200);  // incoming 30

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 2);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 30);

    EXPECT_EQ(book.bestAsk(), nullptr);  // incoming fully filled
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 20);  // 50 - 30 remains
}

// ─────────────────────────────────────────────────────────────────────────────
// 5. MULTI-ORDER MATCHING (same price level - FIFO)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, BuySweepsMultipleOrdersSamePriceFIFO) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 20, 1, 100);  // first
    book.addLimitOrder(Side::Sell, 100, 30, 2, 101);  // second
    book.addLimitOrder(Side::Buy, 100, 40, 3, 200);   // sweeps first fully, second partially

    ASSERT_EQ(trades_.size(), 2);

    // First trade: fills order 1 completely
    EXPECT_EQ(trades_[0].buyOrderId, 3);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

    // Second trade: fills order 2 partially
    EXPECT_EQ(trades_[1].buyOrderId, 3);
    EXPECT_EQ(trades_[1].sellOrderId, 2);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 20);

    EXPECT_EQ(book.bestBid(), nullptr);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 10);  // 30 - 20 remains
}

TEST_F(OrderBookMatchingTest, SellSweepsMultipleOrdersSamePriceFIFO) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 100, 20, 1, 100);   // first
    book.addLimitOrder(Side::Buy, 100, 30, 2, 101);   // second
    book.addLimitOrder(Side::Sell, 100, 40, 3, 200);  // sweeps first fully, second partially

    ASSERT_EQ(trades_.size(), 2);

    // First trade: fills order 1 completely
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 3);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

    // Second trade: fills order 2 partially
    EXPECT_EQ(trades_[1].buyOrderId, 2);
    EXPECT_EQ(trades_[1].sellOrderId, 3);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 20);

    EXPECT_EQ(book.bestAsk(), nullptr);
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 10);  // 30 - 20 remains
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. MULTI-LEVEL MATCHING (price priority)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, BuySweepsMultiplePriceLevelsBestFirst) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 20, 1, 100);  // best ask
    book.addLimitOrder(Side::Sell, 101, 30, 2, 101);  // worse ask
    book.addLimitOrder(Side::Buy, 101, 40, 3, 200);   // sweeps 100@20, then 101@20

    ASSERT_EQ(trades_.size(), 2);

    // First trade at best price (100)
    EXPECT_EQ(trades_[0].buyOrderId, 3);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

    // Second trade at next price (101)
    EXPECT_EQ(trades_[1].buyOrderId, 3);
    EXPECT_EQ(trades_[1].sellOrderId, 2);
    EXPECT_EQ(trades_[1].price, 101);
    EXPECT_EQ(trades_[1].quantity, 20);

    EXPECT_EQ(book.bestBid(), nullptr);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 101);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 10);
}

TEST_F(OrderBookMatchingTest, SellSweepsMultiplePriceLevelsBestFirst) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 101, 20, 1, 100);  // best bid
    book.addLimitOrder(Side::Buy, 100, 30, 2, 101);  // worse bid
    book.addLimitOrder(Side::Sell, 100, 40, 3, 200); // sweeps 101@20, then 100@20

    ASSERT_EQ(trades_.size(), 2);

    // First trade at best price (101)
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 3);
    EXPECT_EQ(trades_[0].price, 101);
    EXPECT_EQ(trades_[0].quantity, 20);

    // Second trade at next price (100)
    EXPECT_EQ(trades_[1].buyOrderId, 2);
    EXPECT_EQ(trades_[1].sellOrderId, 3);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 20);

    EXPECT_EQ(book.bestAsk(), nullptr);
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 10);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. PRICE IMPROVEMENT (aggressive price crosses spread)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, BuyWithPriceImprovementMatchesAtAskPrice) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);
    book.addLimitOrder(Side::Buy, 105, 50, 2, 200);  // willing to pay more

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 2);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);  // trades at resting price, not 105
    EXPECT_EQ(trades_[0].quantity, 50);

    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, SellWithPriceImprovementMatchesAtBidPrice) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);
    book.addLimitOrder(Side::Sell, 95, 50, 2, 200);  // willing to accept less

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 2);
    EXPECT_EQ(trades_[0].price, 100);  // trades at resting price, not 95
    EXPECT_EQ(trades_[0].quantity, 50);

    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// 8. BOOK INTEGRITY AFTER OPERATIONS
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, PriceLevelRemovedWhenAllOrdersFilled) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Sell, 100, 20, 1, 100);
    book.addLimitOrder(Side::Sell, 100, 30, 2, 101);
    book.addLimitOrder(Side::Buy, 100, 50, 3, 200);  // fills both completely

    ASSERT_EQ(trades_.size(), 2);

    // Verify both trades
    EXPECT_EQ(trades_[0].buyOrderId, 3);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

    EXPECT_EQ(trades_[1].buyOrderId, 3);
    EXPECT_EQ(trades_[1].sellOrderId, 2);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 30);

    // Price level 100 should be completely removed
    EXPECT_EQ(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, MultiplePriceLevelsOrdered) {
    OrderBook book(10, captureCallback());

    book.addLimitOrder(Side::Buy, 100, 10, 1, 100);
    book.addLimitOrder(Side::Buy, 102, 10, 2, 101);  // best bid
    book.addLimitOrder(Side::Buy, 101, 10, 3, 102);

    book.addLimitOrder(Side::Sell, 105, 10, 4, 200);
    book.addLimitOrder(Side::Sell, 103, 10, 5, 201);  // best ask
    book.addLimitOrder(Side::Sell, 104, 10, 6, 202);

    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 102);

    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 103);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. SELF-MATCH PREVENTION
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, SelfMatchPreventionBuyCancelsIncoming) {
    OrderBook book(10, captureCallback());

    // Participant 100 places a sell order
    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);

    // Same participant 100 tries to buy - should be cancelled (no trade)
    book.addLimitOrder(Side::Buy, 100, 50, 2, 100);

    EXPECT_TRUE(trades_.empty());  // no trade executed

    // Resting sell order should remain untouched
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 50);

    // No buy order should be resting
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, SelfMatchPreventionSellCancelsIncoming) {
    OrderBook book(10, captureCallback());

    // Participant 100 places a buy order
    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);

    // Same participant 100 tries to sell - should be cancelled (no trade)
    book.addLimitOrder(Side::Sell, 100, 50, 2, 100);

    EXPECT_TRUE(trades_.empty());  // no trade executed

    // Resting buy order should remain untouched
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 50);

    // No sell order should be resting
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, SelfMatchPreventionDifferentParticipantsCanTrade) {
    OrderBook book(10, captureCallback());

    // Participant 100 places a sell order
    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);

    // Different participant 200 buys - should trade normally
    book.addLimitOrder(Side::Buy, 100, 50, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 2);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 50);

    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, SelfMatchPreventionCancelsIncomingWhenOwnOrderAtFront) {
    OrderBook book(10, captureCallback());

    // Participant 100's order is FIRST in the queue (will be matched first due to FIFO)
    book.addLimitOrder(Side::Sell, 100, 30, 1, 100);  // participant 100 - first
    book.addLimitOrder(Side::Sell, 100, 30, 2, 200);  // participant 200 - second

    // Participant 100 tries to buy - hits own order first → SMP cancels incoming
    book.addLimitOrder(Side::Buy, 100, 50, 3, 100);

    // No trades - SMP triggered on first order in queue
    EXPECT_TRUE(trades_.empty());

    // Both resting orders should remain untouched
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 60);  // 30 + 30

    // Incoming buy was cancelled, not rested
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, SelfMatchPreventionBuyAggressivePriceCrossing) {
    OrderBook book(10, captureCallback());

    // Participant 100 has a sell order
    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);

    // Same participant places aggressive buy (price > ask)
    book.addLimitOrder(Side::Buy, 110, 50, 2, 100);

    EXPECT_TRUE(trades_.empty());  // self-match prevented

    // Sell order remains, buy is cancelled
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 50);
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, SelfMatchPreventionSellAggressivePriceCrossing) {
    OrderBook book(10, captureCallback());

    // Participant 100 has a buy order
    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);

    // Same participant places aggressive sell (price < bid)
    book.addLimitOrder(Side::Sell, 90, 50, 2, 100);

    EXPECT_TRUE(trades_.empty());  // self-match prevented

    // Buy order remains, sell is cancelled
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->totalQuantity, 50);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, SelfMatchPreventionPartialFillThenSelfMatchCrossLevel) {
    OrderBook book(10, captureCallback());

    // Participant 200 has a sell order at 100
    book.addLimitOrder(Side::Sell, 100, 20, 1, 200);
    // Participant 100 has a sell order at 101
    book.addLimitOrder(Side::Sell, 101, 30, 2, 100);

    // Participant 100 tries to buy 40 @ 101
    // Should fill 20 with participant 200, then cancel when hitting own order at 101
    book.addLimitOrder(Side::Buy, 101, 40, 3, 100);

    // Only one trade occurred (with participant 200)
    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 3);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

    // After partial fill, remaining buy (20 qty) is cancelled due to self-match at 101
    // Sell order at 101 should remain intact
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 101);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 30);

    // No buy order resting (cancelled, not rested)
    EXPECT_EQ(book.bestBid(), nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: SMP on multi-level book
// Two asks from same participant at different price levels
// Incoming buy that would cross both levels → SMP triggers on first level
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, SelfMatchPreventionMultiLevelBookBuySide) {
    OrderBook book(10, captureCallback());

    // Participant 10 has asks at two price levels
    book.addLimitOrder(Side::Sell, 100, 5, 1, 10);  // best ask
    book.addLimitOrder(Side::Sell, 101, 5, 2, 10);  // worse ask

    // Participant 10 tries to buy @ 101 (would cross both levels)
    // SMP triggers immediately on first level (price 100)
    book.addLimitOrder(Side::Buy, 101, 10, 3, 10);

    // No trades - SMP prevented everything
    EXPECT_TRUE(trades_.empty());

    // Best ask (price 100) should be untouched
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 5);

    // No buy order resting
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, SelfMatchPreventionMultiLevelBookSellSide) {
    OrderBook book(10, captureCallback());

    // Participant 10 has bids at two price levels
    book.addLimitOrder(Side::Buy, 101, 5, 1, 10);  // best bid
    book.addLimitOrder(Side::Buy, 100, 5, 2, 10);  // worse bid

    // Participant 10 tries to sell @ 100 (would cross both levels)
    // SMP triggers immediately on first level (price 101)
    book.addLimitOrder(Side::Sell, 100, 10, 3, 10);

    // No trades - SMP prevented everything
    EXPECT_TRUE(trades_.empty());

    // Best bid (price 101) should be untouched
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 101);
    EXPECT_EQ(book.bestBid()->totalQuantity, 5);

    // No sell order resting
    EXPECT_EQ(book.bestAsk(), nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: SMP triggered mid-loop (same price level, multiple orders)
// Orders at best ask: o1(p77), o2(p77), o3(p99)
// Incoming buy from p99 → matches o1, o2, then SMP on o3
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(OrderBookMatchingTest, SelfMatchPreventionMidLoopBuySide) {
    OrderBook book(20, captureCallback());

    // Three sell orders at same price level from different participants
    book.addLimitOrder(Side::Sell, 100, 5, 1, 77);  // o1: participant 77
    book.addLimitOrder(Side::Sell, 100, 5, 2, 77);  // o2: participant 77
    book.addLimitOrder(Side::Sell, 100, 5, 3, 99);  // o3: participant 99

    // Participant 99 tries to buy 20 @ 100
    // Should: match o1 (5), match o2 (5), hit o3 → SMP → cancel remaining
    book.addLimitOrder(Side::Buy, 100, 20, 4, 99);

    // Two trades occurred (o1 and o2)
    ASSERT_EQ(trades_.size(), 2);

    // First trade: o1 fully filled
    EXPECT_EQ(trades_[0].buyOrderId, 4);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 5);

    // Second trade: o2 fully filled
    EXPECT_EQ(trades_[1].buyOrderId, 4);
    EXPECT_EQ(trades_[1].sellOrderId, 2);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 5);

    // o3 remains in book (SMP prevented match)
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 5);  // only o3 remains

    // Remaining 10 qty was cancelled (not rested)
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, SelfMatchPreventionMidLoopSellSide) {
    OrderBook book(20, captureCallback());

    // Three buy orders at same price level from different participants
    book.addLimitOrder(Side::Buy, 100, 5, 1, 77);  // o1: participant 77
    book.addLimitOrder(Side::Buy, 100, 5, 2, 77);  // o2: participant 77
    book.addLimitOrder(Side::Buy, 100, 5, 3, 99);  // o3: participant 99

    // Participant 99 tries to sell 20 @ 100
    // Should: match o1 (5), match o2 (5), hit o3 → SMP → cancel remaining
    book.addLimitOrder(Side::Sell, 100, 20, 4, 99);

    // Two trades occurred (o1 and o2)
    ASSERT_EQ(trades_.size(), 2);

    // First trade: o1 fully filled
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 4);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 5);

    // Second trade: o2 fully filled
    EXPECT_EQ(trades_[1].buyOrderId, 2);
    EXPECT_EQ(trades_[1].sellOrderId, 4);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 5);

    // o3 remains in book (SMP prevented match)
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 5);  // only o3 remains

    // Remaining 10 qty was cancelled (not rested)
    EXPECT_EQ(book.bestAsk(), nullptr);
}
