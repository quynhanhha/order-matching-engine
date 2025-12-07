#include "order_pool.h"

#include <cassert>

OrderPool::OrderPool(std::size_t capacity)
    : capacity_(capacity),
    orders_(std::make_unique<Order[]>(capacity)),
    freeList_(nullptr),
    freeCount_(capacity)
{
    freeList_ = nullptr;
    for (std::size_t i = 0; i < capacity_; ++i) {
        orders_[i].next = freeList_;
        freeList_ = &orders_[i];
    }
    freeCount_ = capacity_;
}

Order* OrderPool::allocate() {
    assert(freeList_ != nullptr);
    assert(freeCount_ > 0);
    
    Order* order = freeList_;
    freeList_ = order->next;

    --freeCount_;

    order->next = nullptr;
    order->prev = nullptr;

    return order;
}

void OrderPool::deallocate(Order* order) {
    assert(order != nullptr);
    assert(freeCount_ < capacity_);

    order->next = freeList_;
    freeList_ = order;

    ++freeCount_;
}