#pragma once

#include "types.h"
#include <cstdint>

struct Order {
    uint64_t orderId;
    uint32_t price;
    uint32_t quantity;
    uint64_t sequence;
    Side side;

    Order* next;
    Order* prev;
};