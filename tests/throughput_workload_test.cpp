#include <gtest/gtest.h>

#include "../benchmarks/throughput_workloads.h"

namespace workloads = throughput_workloads;

TEST(ThroughputWorkloadTest, RestingTracePreservesAllOrdersWithoutTrades) {
    for (const std::size_t count : {100u, 1000u, 10000u}) {
        std::mt19937_64 rng(42);
        const auto inputs = workloads::generateRestingOrders(count, rng);
        const auto result = workloads::validateRestingOrders(inputs);
        EXPECT_TRUE(result.error.empty()) << result.error;
        EXPECT_EQ(result.tradeCount, 0);
        EXPECT_EQ(result.finalOrders, count);
    }
}

TEST(ThroughputWorkloadTest, MatchingTraceFullyFillsEachIncomingOrderInFIFOOrder) {
    for (const std::size_t count : {100u, 1000u, 10000u}) {
        const auto result = workloads::validateMatchingOrders(workloads::generateMatchingOrders(count));
        EXPECT_TRUE(result.error.empty()) << result.error;
        EXPECT_EQ(result.tradeCount, count / 2);
        EXPECT_EQ(result.finalOrders, count / 2);
    }
}

TEST(ThroughputWorkloadTest, RestingValidationRejectsAccidentalCrossing) {
    const std::vector<workloads::OrderInput> inputs{
        {Side::Sell, 100, 1, 1, 1}, {Side::Buy, 100, 1, 2, 2}};
    const auto result = workloads::validateRestingOrders(inputs);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(result.tradeCount, 1);
}

TEST(ThroughputWorkloadTest, RestingValidationRejectsSilentSMPCancellation) {
    const std::vector<workloads::OrderInput> inputs{
        {Side::Sell, 100, 1, 1, 1}, {Side::Buy, 100, 1, 2, 1}};
    const auto result = workloads::validateRestingOrders(inputs);
    EXPECT_FALSE(result.error.empty());
    EXPECT_EQ(result.tradeCount, 0);
}

TEST(ThroughputWorkloadTest, MatchingValidationRejectsSameParticipant) {
    auto inputs = workloads::generateMatchingOrders(100);
    inputs.incoming.front().participantId = 1;
    EXPECT_FALSE(workloads::validateMatchingOrders(inputs).error.empty());
}

TEST(ThroughputWorkloadTest, MatchingValidationRejectsPartialFillInput) {
    auto inputs = workloads::generateMatchingOrders(100);
    inputs.resting.front().quantity = 2;
    EXPECT_FALSE(workloads::validateMatchingOrders(inputs).error.empty());
}

TEST(ThroughputWorkloadTest, MatchingValidationRejectsNonCrossingInput) {
    auto inputs = workloads::generateMatchingOrders(100);
    inputs.incoming.front().price = 99;
    EXPECT_FALSE(workloads::validateMatchingOrders(inputs).error.empty());
}

TEST(ThroughputWorkloadTest, MatchingValidationRejectsDuplicateIDsBeforeReplay) {
    auto inputs = workloads::generateMatchingOrders(100);
    inputs.incoming.front().id = inputs.resting.front().id;
    EXPECT_FALSE(workloads::validateMatchingOrders(inputs).error.empty());
}

TEST(ThroughputWorkloadTest, MatchingValidationRejectsIncorrectBatchCount) {
    auto inputs = workloads::generateMatchingOrders(100);
    inputs.incoming.pop_back();
    EXPECT_FALSE(workloads::validateMatchingOrders(inputs).error.empty());
}
