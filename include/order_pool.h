#pragma once

#include <cstddef>
#include <memory>
#include "order.h"

class OrderPool {
public:
    explicit OrderPool(std::size_t capacity);

    Order* allocate();
    void deallocate(Order* order);

    std::size_t capacity() const noexcept;
    std::size_t freeCount() const noexcept;

private:
    std::size_t capacity_;
    std::unique_ptr<Order[]> orders_;
    Order* freeList_;
    std::size_t freeCount_;
};