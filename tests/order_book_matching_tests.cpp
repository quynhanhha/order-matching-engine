#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>

#include "order_book.h"

class OrderBookMatchingTest : public ::testing::Test {
protected:
    std::vector<Trade> trades_;

    void SetUp() override {
        trades_.clear();
    }

    auto makeBook(std::size_t capacity = 10) {
        return OrderBook(capacity, [this](const Trade& t) { trades_.push_back(t); });
    }
};

TEST_F(OrderBookMatchingTest, BuyOrderRestsWhenNoAsks) {
    auto book = makeBook();

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);

    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 50);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, SellOrderRestsWhenNoBids) {
    auto book = makeBook();

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);

    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 50);
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, BuyOrderRestsWhenPriceBelowBestAsk) {
    auto book = makeBook();

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);
    book.addLimitOrder(Side::Buy, 99, 50, 2, 200);

    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 99);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
}

TEST_F(OrderBookMatchingTest, SellOrderRestsWhenPriceAboveBestBid) {
    auto book = makeBook();

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);
    book.addLimitOrder(Side::Sell, 101, 50, 2, 200);

    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 101);
}

TEST_F(OrderBookMatchingTest, BuyExactlyFillsSell) {
    auto book = makeBook();

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
    auto book = makeBook();

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

TEST_F(OrderBookMatchingTest, BuyPartiallyFillsRemainderRests) {
    auto book = makeBook();

    book.addLimitOrder(Side::Sell, 100, 30, 1, 100);
    book.addLimitOrder(Side::Buy, 100, 50, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 2);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 30);

    EXPECT_EQ(book.bestAsk(), nullptr);
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 20);
}

TEST_F(OrderBookMatchingTest, SellPartiallyFillsRemainderRests) {
    auto book = makeBook();

    book.addLimitOrder(Side::Buy, 100, 30, 1, 100);
    book.addLimitOrder(Side::Sell, 100, 50, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 2);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 30);

    EXPECT_EQ(book.bestBid(), nullptr);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 20);
}

TEST_F(OrderBookMatchingTest, BuyPartiallyFillsRestingRemains) {
    auto book = makeBook();

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);
    book.addLimitOrder(Side::Buy, 100, 30, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 2);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 30);

    EXPECT_EQ(book.bestBid(), nullptr);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 20);
}

TEST_F(OrderBookMatchingTest, SellPartiallyFillsRestingRemains) {
    auto book = makeBook();

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);
    book.addLimitOrder(Side::Sell, 100, 30, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 2);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 30);

    EXPECT_EQ(book.bestAsk(), nullptr);
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 20);
}

TEST_F(OrderBookMatchingTest, BuySweepsMultipleOrdersSamePriceFIFO) {
    auto book = makeBook();

    book.addLimitOrder(Side::Sell, 100, 20, 1, 100);
    book.addLimitOrder(Side::Sell, 100, 30, 2, 101);
    book.addLimitOrder(Side::Buy, 100, 40, 3, 200);

    ASSERT_EQ(trades_.size(), 2);

    EXPECT_EQ(trades_[0].buyOrderId, 3);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

    EXPECT_EQ(trades_[1].buyOrderId, 3);
    EXPECT_EQ(trades_[1].sellOrderId, 2);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 20);

    EXPECT_EQ(book.bestBid(), nullptr);
    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 100);
    EXPECT_EQ(book.bestAsk()->totalQuantity, 10);
}

TEST_F(OrderBookMatchingTest, SellSweepsMultipleOrdersSamePriceFIFO) {
    auto book = makeBook();

    book.addLimitOrder(Side::Buy, 100, 20, 1, 100);
    book.addLimitOrder(Side::Buy, 100, 30, 2, 101);
    book.addLimitOrder(Side::Sell, 100, 40, 3, 200);

    ASSERT_EQ(trades_.size(), 2);

    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 3);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

    EXPECT_EQ(trades_[1].buyOrderId, 2);
    EXPECT_EQ(trades_[1].sellOrderId, 3);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 20);

    EXPECT_EQ(book.bestAsk(), nullptr);
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 10);
}

TEST_F(OrderBookMatchingTest, BuySweepsMultiplePriceLevelsBestFirst) {
    auto book = makeBook();

    book.addLimitOrder(Side::Sell, 100, 20, 1, 100);
    book.addLimitOrder(Side::Sell, 101, 30, 2, 101);
    book.addLimitOrder(Side::Buy, 101, 40, 3, 200);

    ASSERT_EQ(trades_.size(), 2);

    EXPECT_EQ(trades_[0].buyOrderId, 3);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

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
    auto book = makeBook();

    book.addLimitOrder(Side::Buy, 101, 20, 1, 100);
    book.addLimitOrder(Side::Buy, 100, 30, 2, 101);
    book.addLimitOrder(Side::Sell, 100, 40, 3, 200);

    ASSERT_EQ(trades_.size(), 2);

    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 3);
    EXPECT_EQ(trades_[0].price, 101);
    EXPECT_EQ(trades_[0].quantity, 20);

    EXPECT_EQ(trades_[1].buyOrderId, 2);
    EXPECT_EQ(trades_[1].sellOrderId, 3);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 20);

    EXPECT_EQ(book.bestAsk(), nullptr);
    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 100);
    EXPECT_EQ(book.bestBid()->totalQuantity, 10);
}

TEST_F(OrderBookMatchingTest, BuyWithPriceImprovementMatchesAtAskPrice) {
    auto book = makeBook();

    book.addLimitOrder(Side::Sell, 100, 50, 1, 100);
    book.addLimitOrder(Side::Buy, 105, 50, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 2);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 50);

    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, SellWithPriceImprovementMatchesAtBidPrice) {
    auto book = makeBook();

    book.addLimitOrder(Side::Buy, 100, 50, 1, 100);
    book.addLimitOrder(Side::Sell, 95, 50, 2, 200);

    ASSERT_EQ(trades_.size(), 1);
    EXPECT_EQ(trades_[0].buyOrderId, 1);
    EXPECT_EQ(trades_[0].sellOrderId, 2);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 50);

    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_F(OrderBookMatchingTest, PriceLevelRemovedWhenAllOrdersFilled) {
    auto book = makeBook();

    book.addLimitOrder(Side::Sell, 100, 20, 1, 100);
    book.addLimitOrder(Side::Sell, 100, 30, 2, 101);
    book.addLimitOrder(Side::Buy, 100, 50, 3, 200);

    ASSERT_EQ(trades_.size(), 2);

    EXPECT_EQ(trades_[0].buyOrderId, 3);
    EXPECT_EQ(trades_[0].sellOrderId, 1);
    EXPECT_EQ(trades_[0].price, 100);
    EXPECT_EQ(trades_[0].quantity, 20);

    EXPECT_EQ(trades_[1].buyOrderId, 3);
    EXPECT_EQ(trades_[1].sellOrderId, 2);
    EXPECT_EQ(trades_[1].price, 100);
    EXPECT_EQ(trades_[1].quantity, 30);

    EXPECT_EQ(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestBid(), nullptr);
}

TEST_F(OrderBookMatchingTest, BuyExhaustsFinalAskAndMatchesAgainAfterEmptyBook) {
    auto book = makeBook(2);

    for (uint64_t round = 0; round < 3; ++round) {
        const uint64_t sellId = round * 2 + 1;
        const uint64_t buyId = sellId + 1;
        trades_.clear();

        book.addLimitOrder(Side::Sell, 100, 10, sellId, 100);
        book.addLimitOrder(Side::Buy, 100, 10, buyId, 200);

        ASSERT_EQ(trades_.size(), 1);
        EXPECT_EQ(trades_[0].buyOrderId, buyId);
        EXPECT_EQ(trades_[0].sellOrderId, sellId);
        EXPECT_EQ(trades_[0].price, 100);
        EXPECT_EQ(trades_[0].quantity, 10);
        EXPECT_EQ(book.bestAsk(), nullptr);
        EXPECT_EQ(book.bestBid(), nullptr);

        // Filled IDs must no longer be indexed before the pool slots are reused.
        book.cancelOrder(sellId);
        book.cancelOrder(buyId);
    }
}

TEST_F(OrderBookMatchingTest, SellExhaustsFinalBidAndMatchesAgainAfterEmptyBook) {
    auto book = makeBook(2);

    for (uint64_t round = 0; round < 3; ++round) {
        const uint64_t buyId = round * 2 + 1;
        const uint64_t sellId = buyId + 1;
        trades_.clear();

        book.addLimitOrder(Side::Buy, 100, 10, buyId, 100);
        book.addLimitOrder(Side::Sell, 100, 10, sellId, 200);

        ASSERT_EQ(trades_.size(), 1);
        EXPECT_EQ(trades_[0].buyOrderId, buyId);
        EXPECT_EQ(trades_[0].sellOrderId, sellId);
        EXPECT_EQ(trades_[0].price, 100);
        EXPECT_EQ(trades_[0].quantity, 10);
        EXPECT_EQ(book.bestBid(), nullptr);
        EXPECT_EQ(book.bestAsk(), nullptr);

        book.cancelOrder(buyId);
        book.cancelOrder(sellId);
    }
}

TEST_F(OrderBookMatchingTest, BuyExhaustsAllAskLevelsAndRemainderMatchesAgain) {
    auto book = makeBook(4);

    // Exercise exhaustion with both zero and positive incoming quantity remaining.
    for (const uint32_t quantity : {40u, 45u}) {
        trades_.clear();
        book.addLimitOrder(Side::Sell, 101, 10, 1, 100);  // Worse price inserted first.
        book.addLimitOrder(Side::Sell, 100, 10, 2, 100);
        book.addLimitOrder(Side::Sell, 100, 20, 3, 100);  // Same-price FIFO successor.
        book.addLimitOrder(Side::Buy, 101, quantity, 4, 200);

        ASSERT_EQ(trades_.size(), 3);
        EXPECT_EQ(trades_[0].sellOrderId, 2);
        EXPECT_EQ(trades_[1].sellOrderId, 3);
        EXPECT_EQ(trades_[2].sellOrderId, 1);
        EXPECT_EQ(trades_[0].price, 100);
        EXPECT_EQ(trades_[1].price, 100);
        EXPECT_EQ(trades_[2].price, 101);
        EXPECT_EQ(trades_[0].quantity, 10);
        EXPECT_EQ(trades_[1].quantity, 20);
        EXPECT_EQ(trades_[2].quantity, 10);
        for (const auto& trade : trades_) {
            EXPECT_EQ(trade.buyOrderId, 4);
        }
        EXPECT_EQ(book.bestAsk(), nullptr);

        if (quantity > 40) {
            ASSERT_NE(book.bestBid(), nullptr);
            EXPECT_EQ(book.bestBid()->price, 101);
            EXPECT_EQ(book.bestBid()->totalQuantity, 5);
            book.addLimitOrder(Side::Sell, 101, 5, 5, 300);
            ASSERT_EQ(trades_.size(), 4);
            EXPECT_EQ(trades_[3].buyOrderId, 4);
            EXPECT_EQ(trades_[3].sellOrderId, 5);
            EXPECT_EQ(trades_[3].price, 101);
            EXPECT_EQ(trades_[3].quantity, 5);
        }
        EXPECT_EQ(book.bestBid(), nullptr);
        EXPECT_EQ(book.bestAsk(), nullptr);
    }
}

TEST_F(OrderBookMatchingTest, SellExhaustsAllBidLevelsAndRemainderMatchesAgain) {
    auto book = makeBook(4);

    for (const uint32_t quantity : {40u, 45u}) {
        trades_.clear();
        book.addLimitOrder(Side::Buy, 100, 10, 1, 100);
        book.addLimitOrder(Side::Buy, 101, 10, 2, 100);
        book.addLimitOrder(Side::Buy, 101, 20, 3, 100);
        book.addLimitOrder(Side::Sell, 100, quantity, 4, 200);

        ASSERT_EQ(trades_.size(), 3);
        EXPECT_EQ(trades_[0].buyOrderId, 2);
        EXPECT_EQ(trades_[1].buyOrderId, 3);
        EXPECT_EQ(trades_[2].buyOrderId, 1);
        EXPECT_EQ(trades_[0].price, 101);
        EXPECT_EQ(trades_[1].price, 101);
        EXPECT_EQ(trades_[2].price, 100);
        EXPECT_EQ(trades_[0].quantity, 10);
        EXPECT_EQ(trades_[1].quantity, 20);
        EXPECT_EQ(trades_[2].quantity, 10);
        for (const auto& trade : trades_) {
            EXPECT_EQ(trade.sellOrderId, 4);
        }
        EXPECT_EQ(book.bestBid(), nullptr);

        if (quantity > 40) {
            ASSERT_NE(book.bestAsk(), nullptr);
            EXPECT_EQ(book.bestAsk()->price, 100);
            EXPECT_EQ(book.bestAsk()->totalQuantity, 5);
            book.addLimitOrder(Side::Buy, 100, 5, 5, 300);
            ASSERT_EQ(trades_.size(), 4);
            EXPECT_EQ(trades_[3].buyOrderId, 5);
            EXPECT_EQ(trades_[3].sellOrderId, 4);
            EXPECT_EQ(trades_[3].price, 100);
            EXPECT_EQ(trades_[3].quantity, 5);
        }
        EXPECT_EQ(book.bestAsk(), nullptr);
        EXPECT_EQ(book.bestBid(), nullptr);
    }
}

TEST_F(OrderBookMatchingTest, MultiplePriceLevelsOrdered) {
    auto book = makeBook();

    book.addLimitOrder(Side::Buy, 100, 10, 1, 100);
    book.addLimitOrder(Side::Buy, 102, 10, 2, 101);
    book.addLimitOrder(Side::Buy, 101, 10, 3, 102);

    book.addLimitOrder(Side::Sell, 105, 10, 4, 200);
    book.addLimitOrder(Side::Sell, 103, 10, 5, 201);
    book.addLimitOrder(Side::Sell, 104, 10, 6, 202);

    ASSERT_NE(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestBid()->price, 102);

    ASSERT_NE(book.bestAsk(), nullptr);
    EXPECT_EQ(book.bestAsk()->price, 103);
}

class DuplicateOrderIdTest : public OrderBookMatchingTest,
                             public ::testing::WithParamInterface<Side> {
protected:
    Side oppositeSide() const {
        return GetParam() == Side::Buy ? Side::Sell : Side::Buy;
    }

    template<typename Callback>
    const PriceLevel* best(const OrderBook<Callback>& book) const {
        return GetParam() == Side::Buy ? book.bestBid() : book.bestAsk();
    }

    uint32_t worsePrice() const {
        return GetParam() == Side::Buy ? 99u : 101u;
    }
};

TEST_P(DuplicateOrderIdTest, RejectsRestingDuplicatesWithoutChangingEitherSide) {
    auto book = makeBook(4);
    const uint32_t opposingPrice = GetParam() == Side::Buy ? 110u : 90u;
    book.addLimitOrder(GetParam(), 100, 10, 1, 100);
    book.addLimitOrder(GetParam(), 100, 20, 2, 200);
    book.addLimitOrder(oppositeSide(), opposingPrice, 5, 3, 300);
    const Order* first = best(book)->head;
    const Order* second = best(book)->tail;
    const PriceLevel* opposing = GetParam() == Side::Buy ? book.bestAsk() : book.bestBid();
    const Order* third = opposing->head;

    for (const uint32_t price : {99u, 100u, 101u}) {
        SCOPED_TRACE(price);
        EXPECT_THROW(book.addLimitOrder(GetParam(), price, 7, 1, 400), std::invalid_argument);
    }
    EXPECT_THROW(book.addLimitOrder(oppositeSide(), opposingPrice, 7, 1, 400),
                 std::invalid_argument);
    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(best(book), nullptr);
    EXPECT_EQ(best(book)->price, 100);
    EXPECT_EQ(best(book)->totalQuantity, 30);
    EXPECT_EQ(best(book)->head, first);
    EXPECT_EQ(best(book)->tail, second);
    EXPECT_EQ(first->quantity, 10);
    EXPECT_EQ(first->sequence, 0);
    EXPECT_EQ(first->prev, nullptr);
    EXPECT_EQ(first->next, second);
    EXPECT_EQ(second->quantity, 20);
    EXPECT_EQ(second->sequence, 1);
    EXPECT_EQ(second->prev, first);
    EXPECT_EQ(second->next, nullptr);
    opposing = GetParam() == Side::Buy ? book.bestAsk() : book.bestBid();
    ASSERT_NE(opposing, nullptr);
    EXPECT_EQ(opposing->price, opposingPrice);
    EXPECT_EQ(opposing->totalQuantity, 5);
    EXPECT_EQ(opposing->head, third);
    EXPECT_EQ(opposing->tail, third);
    EXPECT_EQ(third->quantity, 5);
    EXPECT_EQ(third->sequence, 2);

    book.addLimitOrder(GetParam(), 100, 4, 4, 500);
    EXPECT_EQ(best(book)->tail->sequence, 3); // Rejections consume neither slots nor sequences.
    book.cancelOrder(1);
    EXPECT_EQ(best(book)->head, second);
    EXPECT_EQ(best(book)->totalQuantity, 24);
    book.cancelOrder(2);
    EXPECT_EQ(best(book)->totalQuantity, 4);
    book.cancelOrder(3);
    book.cancelOrder(4);
    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_P(DuplicateOrderIdTest, RejectsCrossingDuplicatesBeforeFillsOrImmediateSMP) {
    for (const uint32_t quantity : {5u, 10u, 15u}) {
        for (const uint64_t participant : {100u, 200u}) {
            SCOPED_TRACE(quantity);
            SCOPED_TRACE(participant);
            auto book = makeBook(2);
            book.addLimitOrder(GetParam(), 100, 10, 1, 100);
            const Order* original = best(book)->head;
            EXPECT_THROW(book.addLimitOrder(oppositeSide(), 100, quantity, 1, participant),
                         std::invalid_argument);
            EXPECT_TRUE(trades_.empty());
            ASSERT_NE(best(book), nullptr);
            EXPECT_EQ(best(book)->head, original);
            EXPECT_EQ(best(book)->tail, original);
            EXPECT_EQ(best(book)->totalQuantity, 10);
            EXPECT_EQ(original->quantity, 10);
            book.cancelOrder(1);
            EXPECT_EQ(book.bestBid(), nullptr);
            EXPECT_EQ(book.bestAsk(), nullptr);
        }
    }
}

TEST_P(DuplicateOrderIdTest, PreservesIndexAndFIFOWhenDuplicateWouldTradeBeforeSMP) {
    auto book = makeBook(4);
    book.addLimitOrder(GetParam(), worsePrice(), 10, 1, 100);
    book.addLimitOrder(GetParam(), 100, 5, 2, 200);
    book.addLimitOrder(GetParam(), 100, 7, 3, 200);
    const Order* first = best(book)->head;
    const Order* second = best(book)->tail;

    // The historical same-side orphan would have rested ahead of the indexed original.
    EXPECT_THROW(book.addLimitOrder(GetParam(), 100, 3, 1, 300), std::invalid_argument);
    for (const uint32_t quantity : {4u, 12u, 30u}) {
        for (const uint64_t participant : {100u, 300u}) {
            SCOPED_TRACE(quantity);
            SCOPED_TRACE(participant);
            EXPECT_THROW(book.addLimitOrder(oppositeSide(), worsePrice(), quantity, 1, participant),
                         std::invalid_argument);
        }
    }
    EXPECT_TRUE(trades_.empty());
    ASSERT_NE(best(book), nullptr);
    EXPECT_EQ(best(book)->totalQuantity, 12);
    EXPECT_EQ(best(book)->head, first);
    EXPECT_EQ(best(book)->tail, second);
    EXPECT_EQ(first->quantity, 5);
    EXPECT_EQ(first->next, second);
    EXPECT_EQ(second->quantity, 7);
    EXPECT_EQ(second->prev, first);

    book.addLimitOrder(oppositeSide(), 100, 6, 4, 300);
    ASSERT_EQ(trades_.size(), 2);
    EXPECT_EQ(GetParam() == Side::Buy ? trades_[0].buyOrderId : trades_[0].sellOrderId, 2);
    EXPECT_EQ(GetParam() == Side::Buy ? trades_[1].buyOrderId : trades_[1].sellOrderId, 3);
    EXPECT_EQ(trades_[0].quantity, 5);
    EXPECT_EQ(trades_[1].quantity, 1);
    EXPECT_EQ(best(book)->head, second);
    EXPECT_EQ(best(book)->totalQuantity, 6);
    book.cancelOrder(3);
    ASSERT_NE(best(book), nullptr);
    EXPECT_EQ(best(book)->price, worsePrice());
    EXPECT_EQ(best(book)->head->orderId, 1);
    EXPECT_EQ(best(book)->totalQuantity, 10);
    book.cancelOrder(1); // Filling valid neighboring liquidity must not erase this entry.
    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_P(DuplicateOrderIdTest, RejectsBeforeAllocationEvenWhenPoolIsFullAndAllowsReuse) {
    auto book = makeBook(1);
    for (uint64_t sequence = 0; sequence < 3; ++sequence) {
        book.addLimitOrder(GetParam(), 100, 10, 1, 100);
        EXPECT_EQ(best(book)->head->sequence, sequence);
        EXPECT_THROW(book.addLimitOrder(GetParam(), 100, 5, 1, 200), std::invalid_argument);
        EXPECT_THROW(book.addLimitOrder(oppositeSide(), 100, 5, 1, 200), std::invalid_argument);
        EXPECT_EQ(best(book)->totalQuantity, 10);
        book.cancelOrder(1);
        book.cancelOrder(1);
        EXPECT_EQ(book.bestBid(), nullptr);
        EXPECT_EQ(book.bestAsk(), nullptr);
    }
    EXPECT_TRUE(trades_.empty());
}

TEST_P(DuplicateOrderIdTest, KeepsPartialRemaindersReservedAndReusesExecutedIds) {
    auto book = makeBook(2);
    book.addLimitOrder(GetParam(), 100, 10, 1, 100);
    book.addLimitOrder(oppositeSide(), 100, 4, 2, 200);
    ASSERT_EQ(trades_.size(), 1);
    EXPECT_THROW(book.addLimitOrder(GetParam(), 100, 5, 1, 300), std::invalid_argument);
    EXPECT_EQ(trades_.size(), 1);
    EXPECT_EQ(best(book)->totalQuantity, 6);
    book.addLimitOrder(oppositeSide(), 100, 6, 2, 200); // Fully executed incoming ID is reusable.
    ASSERT_EQ(trades_.size(), 2);
    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);

    book.addLimitOrder(GetParam(), 100, 8, 1, 100); // Fully executed resting ID is reusable.
    book.addLimitOrder(oppositeSide(), 100, 10, 3, 200);
    ASSERT_EQ(trades_.size(), 3);
    EXPECT_THROW(book.addLimitOrder(GetParam(), 100, 2, 3, 300), std::invalid_argument);
    EXPECT_EQ(trades_.size(), 3);
    const PriceLevel* remainder = GetParam() == Side::Buy ? book.bestAsk() : book.bestBid();
    ASSERT_NE(remainder, nullptr);
    EXPECT_EQ(remainder->head->orderId, 3);
    EXPECT_EQ(remainder->totalQuantity, 2);
    book.cancelOrder(3);
    book.addLimitOrder(GetParam(), 100, 2, 3, 300);
    book.cancelOrder(3);
    EXPECT_EQ(book.bestBid(), nullptr);
    EXPECT_EQ(book.bestAsk(), nullptr);
}

TEST_P(DuplicateOrderIdTest, ReusesIncomingIdsAfterSMPWithOrWithoutEarlierFills) {
    for (const bool earlierFill : {false, true}) {
        SCOPED_TRACE(earlierFill);
        trades_.clear();
        auto book = makeBook(3);
        if (earlierFill) {
            book.addLimitOrder(GetParam(), 100, 5, 2, 200);
        }
        book.addLimitOrder(GetParam(), worsePrice(), 10, 1, 100);
        book.addLimitOrder(oppositeSide(), worsePrice(), 20, 3, 100);
        EXPECT_EQ(trades_.size(), earlierFill ? 1u : 0u);
        ASSERT_NE(best(book), nullptr);
        EXPECT_EQ(best(book)->head->orderId, 1);
        EXPECT_EQ(best(book)->totalQuantity, 10);
        book.addLimitOrder(GetParam(), worsePrice(), 4, 3, 300);
        EXPECT_EQ(best(book)->tail->orderId, 3);
        EXPECT_EQ(best(book)->totalQuantity, 14);
        book.cancelOrder(1);
        book.cancelOrder(3);
        EXPECT_EQ(book.bestBid(), nullptr);
        EXPECT_EQ(book.bestAsk(), nullptr);
    }
}

INSTANTIATE_TEST_SUITE_P(BothSides, DuplicateOrderIdTest,
                        ::testing::Values(Side::Buy, Side::Sell));
