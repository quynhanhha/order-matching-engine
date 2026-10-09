#pragma once

#include "order_book.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace throughput_workloads {

struct OrderInput {
    Side side;
    uint32_t price;
    uint32_t quantity;
    uint64_t id;
    uint64_t participantId;
};

struct ValidationResult {
    std::string error;
    std::size_t tradeCount = 0;
    std::size_t finalOrders = 0;
};

inline std::vector<OrderInput> generateRestingOrders(
    std::size_t count, std::mt19937_64& rng,
    uint32_t bidStart = 90, uint32_t askStart = 110) {
    std::vector<OrderInput> inputs;
    inputs.reserve(count);
    std::uniform_int_distribution<uint32_t> qtyDist(1, 100);
    std::uniform_int_distribution<uint32_t> priceDist(0, 9);
    std::uniform_int_distribution<uint64_t> partDist(1, 100);
    for (std::size_t i = 0; i < count; ++i) {
        const bool isBuy = (i % 2 == 0);
        const uint32_t basePrice = isBuy ? bidStart : askStart;
        inputs.push_back({isBuy ? Side::Buy : Side::Sell,
                          basePrice + priceDist(rng), qtyDist(rng),
                          i + 1, partDist(rng)});
    }
    return inputs;
}

struct MatchingInputs {
    std::vector<OrderInput> resting;
    std::vector<OrderInput> incoming;
};

inline MatchingInputs generateMatchingOrders(std::size_t numResting) {
    MatchingInputs inputs;
    inputs.resting.reserve(numResting);
    inputs.incoming.reserve(numResting / 2);
    for (std::size_t i = 0; i < numResting; ++i) {
        inputs.resting.push_back({Side::Sell, 100, 1, i + 1, 1});
    }
    for (std::size_t i = 0; i < numResting / 2; ++i) {
        inputs.incoming.push_back({Side::Buy, 100, 1, numResting + i + 1, 2});
    }
    return inputs;
}

// Destructively inspect every level and its FIFO, then cancel each validated order.
// This runs only in untimed preflight, with the same inputs and pool capacity.
template<typename Callback>
inline std::string validateAndDrain(
    OrderBook<Callback>& book, const std::vector<OrderInput>& expected) {
    for (const Side side : {Side::Buy, Side::Sell}) {
        std::vector<const OrderInput*> ordered;
        for (const auto& input : expected) {
            if (input.side == side) ordered.push_back(&input);
        }
        std::stable_sort(ordered.begin(), ordered.end(), [side](const auto* a, const auto* b) {
            return side == Side::Buy ? a->price > b->price : a->price < b->price;
        });
        std::size_t index = 0;
        while (index < ordered.size()) {
            const auto* level = side == Side::Buy ? book.bestBid() : book.bestAsk();
            if (!level || level->price != ordered[index]->price) {
                return "final book price levels differ from expected";
            }
            const std::size_t begin = index;
            const uint32_t price = level->price;
            const Order* node = level->head;
            const Order* previous = nullptr;
            uint64_t quantity = 0;
            while (index < ordered.size() && ordered[index]->price == price) {
                const auto& input = *ordered[index];
                if (!node || node->orderId != input.id || node->side != input.side ||
                    node->price != input.price || node->quantity != input.quantity ||
                    node->participantId != input.participantId || node->prev != previous) {
                    return "final book FIFO/order contents differ from expected";
                }
                quantity += node->quantity;
                previous = node;
                node = node->next;
                ++index;
            }
            if (node || level->tail != previous || level->totalQuantity != quantity) {
                return "final book level quantity or links differ from expected";
            }
            // Do not use the level pointer after cancellation can erase the level.
            for (std::size_t i = begin; i < index; ++i) book.cancelOrder(ordered[i]->id);
        }
        if ((side == Side::Buy ? book.bestBid() : book.bestAsk()) != nullptr) {
            return "unexpected orders remain after validation";
        }
    }
    return {};
}

inline ValidationResult validateRestingOrders(const std::vector<OrderInput>& inputs) {
    ValidationResult result;
    OrderBook book(inputs.size() + 100, [&result](const Trade&) { ++result.tradeCount; });
    for (const auto& input : inputs) {
        if (input.quantity == 0) return {"zero-quantity resting input"};
        book.addLimitOrder(input.side, input.price, input.quantity, input.id, input.participantId);
    }
    if (result.tradeCount != 0) return {"resting workload generated trades", result.tradeCount};
    result.error = validateAndDrain(book, inputs);
    if (result.error.empty()) result.finalOrders = inputs.size();
    // Every submitted order must exist with its full quantity: an SMP cancellation fails this check.
    return result;
}

inline ValidationResult validateMatchingOrders(const MatchingInputs& inputs) {
    const std::size_t numResting = inputs.resting.size();
    const std::size_t numIncoming = inputs.incoming.size();
    if (numResting == 0 || numIncoming != numResting / 2) {
        return {"one-to-one workload requires N resting orders and N/2 incoming orders"};
    }
    // Check unique IDs before replay: duplicate indexing can corrupt cancellation.
    std::vector<uint64_t> ids;
    ids.reserve(numResting + numIncoming);
    for (const auto& order : inputs.resting) ids.push_back(order.id);
    for (const auto& order : inputs.incoming) ids.push_back(order.id);
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
        return {"duplicate matching order IDs"};
    }
    std::vector<Trade> trades;
    trades.reserve(numIncoming);
    OrderBook book(numResting + numIncoming + 100,
                   [&trades](const Trade& trade) { trades.push_back(trade); });
    for (const auto& order : inputs.resting) {
        if (order.side != Side::Sell || order.price != 100 || order.quantity != 1 ||
            order.participantId != 1) return {"invalid one-to-one resting input"};
        book.addLimitOrder(order.side, order.price, order.quantity, order.id, order.participantId);
    }
    if (!trades.empty() || book.bestBid() || !book.bestAsk() ||
        book.bestAsk()->totalQuantity != numResting) return {"invalid initial matching book"};
    for (const auto& order : inputs.incoming) {
        if (order.side != Side::Buy || order.price != 100 || order.quantity != 1 ||
            order.participantId != 2) return {"invalid one-to-one incoming input"};
        book.addLimitOrder(order.side, order.price, order.quantity, order.id, order.participantId);
    }
    if (trades.size() != numIncoming) return {"one-to-one full-fill count mismatch", trades.size()};
    for (std::size_t i = 0; i < numIncoming; ++i) {
        if (trades[i].buyOrderId != inputs.incoming[i].id ||
            trades[i].sellOrderId != inputs.resting[i].id ||
            trades[i].price != 100 || trades[i].quantity != 1) {
            return {"one-to-one trade/FIFO mismatch", trades.size()};
        }
    }
    // Every incoming order filled exactly once against a different participant: no SMP event.
    const std::vector<OrderInput> remaining(inputs.resting.begin() +
        static_cast<std::ptrdiff_t>(numIncoming), inputs.resting.end());
    ValidationResult result{validateAndDrain(book, remaining), trades.size(), remaining.size()};
    return result;
}

}  // namespace throughput_workloads
