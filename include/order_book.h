#pragma once

#include "order_pool.h"
#include "price_level.h"

class OrderBook {
public:
    OrderBook(std::size_t capacity, TradeCallback callback);

    void addLimitOrder(Side side, uint32_t price, uint32_t quantity, uint64_t id, uint64_t participantId);
    void cancelOrder(uint64_t orderId);

    const PriceLevel* bestBid() const;
    const PriceLevel* bestAsk() const;

private:
    OrderPool pool_;
    TradeCallback onTrade_;
    std::vector<PriceLevel> bids_; // ascending
    std::vector<PriceLevel> asks_; // descending
    std::unordered_map<uint64_t, Order*> orderIndex_;
    uint64_t sequence_ = 0;

    void matchBuy(Order* incoming);
    void matchSell(Order* incoming);
    PriceLevel* findOrCreateBidLevel(uint32_t price);
    PriceLevel* findOrCreateAskLevel(uint32_t price);
};