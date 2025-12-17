#include "order_book.h"

OrderBook::OrderBook(std::size_t capacity, TradeCallback callback)
    :pool_(capacity), onTrade_(callback)
{
    orderIndex_.reserve(capacity);
}

const PriceLevel* OrderBook::bestBid() const {
    return bids_.empty() ? nullptr : &bids_.back();
}

const PriceLevel* OrderBook::bestAsk() const {
    return asks_.empty() ? nullptr : &asks_.back();
}

void OrderBook::addLimitOrder(Side side, uint32_t price, uint32_t quantity, uint64_t id, uint64_t participantId) {
    Order* order = pool_.allocate();
    order->init(id, price, quantity, sequence_++, side, participantId);

    if (side == Side::Buy) {
        if (bestAsk() != nullptr && price >= bestAsk()->price) {
            matchBuy(order);
        }
    } else if (side == Side::Sell) {
        if (bestBid() != nullptr && price <= bestBid()->price) {
            matchSell(order);
        }
    }

    if (order->quantity > 0) {
        if (side == Side::Buy) {
            PriceLevel* pl = findOrCreateBidLevel(price);
            pl->addToTail(order);
        } else if (side == Side::Sell) {
            PriceLevel* pl = findOrCreateAskLevel(price);
            pl->addToTail(order);
        }
        orderIndex_[id] = order;
    } else {
        pool_.deallocate(order);
    }
}

void OrderBook::matchBuy(Order* incoming) {
    while (incoming->quantity > 0 && !asks_.empty()) {
        PriceLevel* pl = &asks_.back();
        if (incoming->price < pl->price) { break; }

        Order* resting = pl->front();

        if (resting->participantId == incoming->participantId) {
            // Self-match prevention: cancel the incoming order
            incoming->quantity = 0;
            return;
        }

        uint32_t fillQty = std::min(incoming->quantity, resting->quantity);

        incoming->quantity -= fillQty;
        resting->quantity -= fillQty;
        pl->totalQuantity -= fillQty;

        Trade trade(incoming->orderId, resting->orderId, pl->price, fillQty);
        onTrade_(trade);
        
        if (resting->quantity == 0) {
            pl->remove(resting);
            orderIndex_.erase(resting->orderId);
            pool_.deallocate(resting);
        }

        if (pl->isEmpty()) {
            asks_.pop_back();
        }
    }
}

void OrderBook::matchSell(Order* incoming) {
    while (incoming->quantity > 0 && !bids_.empty()) {
        PriceLevel* pl = &bids_.back();
        if (incoming->price > pl->price) { break; }

        Order* resting = pl->front();

        if (resting->participantId == incoming->participantId) {
            // Self-match prevention: cancel the incoming order
            incoming->quantity = 0;
            return;
        }

        uint32_t fillQty = std::min(incoming->quantity, resting->quantity);

        incoming->quantity -= fillQty;
        resting->quantity -= fillQty;
        pl->totalQuantity -= fillQty;

        Trade trade(resting->orderId, incoming->orderId, pl->price, fillQty);
        onTrade_(trade);

        if (resting->quantity == 0) {
            pl->remove(resting);
            orderIndex_.erase(resting->orderId);
            pool_.deallocate(resting);
        }

        if (pl->isEmpty()) {
            bids_.pop_back();
        }
    }
}

PriceLevel* OrderBook::findOrCreateBidLevel(uint32_t price) {
    auto it = std::lower_bound(bids_.begin(), bids_.end(), price,
    [](const PriceLevel& pl, uint32_t p) {
        return pl.price < p;
    });

    if (it != bids_.end() && it->price == price) {
        return &(*it);
    }

    it = bids_.insert(it, PriceLevel(price));

    return &(*it);
}

PriceLevel* OrderBook::findOrCreateAskLevel(uint32_t price) {
    auto it = std::lower_bound(asks_.begin(), asks_.end(), price,
    [](const PriceLevel& pl, uint32_t p) {
        return pl.price > p;
    });

    if (it != asks_.end() && it->price == price) {
        return &(*it);
    }

    it = asks_.insert(it, PriceLevel(price));

    return &(*it);
}