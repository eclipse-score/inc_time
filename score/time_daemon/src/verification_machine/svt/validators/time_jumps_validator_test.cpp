/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/time_daemon/src/verification_machine/svt/validators/time_jumps_validator.h"

#include "score/time/high_res_steady_time/src/high_res_steady_clock_backend_mock.h"

#include "gmock/gmock.h"
#include <gtest/gtest.h>

namespace score
{
namespace td
{

using namespace std::chrono_literals;

struct TestParams
{
    uint16_t simulated_sequence_id;
    uint64_t simulated_t1;
    uint64_t simulated_t2;
    bool is_expected_time_jump_future;
    bool is_expected_time_jump_past;
};

struct TestScenario
{
    std::vector<TestParams> sequence;
};

class TimeJumpsValidatorParamTest : public ::testing::TestWithParam<TestScenario>
{
  public:
    void SetUp() override {}
    void TearDown() override {}
};

INSTANTIATE_TEST_CASE_P(
    TestIfSynchronizedFlagIsNotCleared,
    TimeJumpsValidatorParamTest,
    ::testing::Values(TestScenario{{
        {0, 0, 0, false, false},                     /* Initial frame*/
        {1, 125'000'000, 125'000'000, false, false}, /* New frame -> No jump */
        {2, 251'000'000, 250'000'000, true, false},  /* New frame -> Jump to future */
        {3, 376'000'000, 375'000'000, true, false},  /* 1st correct frame after jump -> Jump to past  */
        {4, 501'000'000, 375'000'000, false, false}, /* 2nd correct frame after jump -> No jump  */
        {5, 625'000'000, 625'000'000, false, true},  /* New frame -> Jump to past */
        {6, 750'000'000, 750'000'000, false, true},  /* 1st correct frame after jump -> Jump to past  */
        {7, 875'000'000, 875'000'000, false, false}, /* 2nd correct frame after jump -> No jump  */
    }}));

TEST_P(TimeJumpsValidatorParamTest, ValidationTest)
{
    auto mock = std::make_shared<score::time::HighResSteadyClockBackendMock>();

    TimeJumpsValidator validator(score::time::test_utils::ClockTestFactory<score::time::HighResSteadyTime>::Make(mock),
                                 std::chrono::nanoseconds(500'000),
                                 std::chrono::nanoseconds(5'000'000),
                                 2U);

    // Pass synchronized state debouncing
    EXPECT_CALL(*mock, Now())
        // For initial time
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(0)}, {}}))
        // For threshold pass
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(6'000'000'000)}, {}}));

    PtpTimeInfo entry_data{};
    entry_data.status.is_synchronized = true;
    std::ignore = validator.Process(entry_data);
    std::ignore = validator.Process(entry_data);

    for (const auto& parameter : GetParam().sequence)
    {
        SyncFupData in_sync_data{};
        in_sync_data.sequence_id = parameter.simulated_sequence_id;
        in_sync_data.precise_origin_timestamp = parameter.simulated_t1;
        in_sync_data.sync_ingress_timestamp = parameter.simulated_t2;

        std::chrono::nanoseconds cur_ptp_time{0};
        PtpTimeInfo::ReferenceClock::time_point cur_local_time{std::chrono::nanoseconds{0}};
        PtpTimeInfo in_data = {cur_ptp_time, cur_local_time, 0, {}, in_sync_data, {}};
        auto actualData = validator.Process(in_data);
        EXPECT_EQ(actualData.status.is_time_jump_future, parameter.is_expected_time_jump_future);
        EXPECT_EQ(actualData.status.is_time_jump_past, parameter.is_expected_time_jump_past);
    }
}

TEST(TimeJumpsValidatorTest, JumpToPastWithinThresholdIsNotFlagged)
{
    auto mock = std::make_shared<score::time::HighResSteadyClockBackendMock>();

    TimeJumpsValidator validator(score::time::test_utils::ClockTestFactory<score::time::HighResSteadyTime>::Make(mock),
                                 std::chrono::nanoseconds(500'000),
                                 std::chrono::nanoseconds(5'000'000),
                                 2U);

    // Pass synchronized state debouncing
    EXPECT_CALL(*mock, Now())
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(0)}, {}}))
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(6'000'000'000)}, {}}));

    PtpTimeInfo entry_data{};
    entry_data.status.is_synchronized = true;
    std::ignore = validator.Process(entry_data);
    std::ignore = validator.Process(entry_data);

    SyncFupData baseline_sync{};
    baseline_sync.sequence_id = 0U;
    baseline_sync.precise_origin_timestamp = 1'000'000'000ULL;
    baseline_sync.sync_ingress_timestamp = 1'000'000'000ULL;
    PtpTimeInfo baseline_data{};
    baseline_data.sync_fup_data = baseline_sync;
    std::ignore = validator.Process(baseline_data);

    // predicted_origin = last_t1 + t2_diff = 1'000'000'000 + 125'000'000 = 1'125'000'000
    // current_jump_to_past = predicted_origin - current_t1 = 100'000, which is <= the 500'000 threshold
    SyncFupData next_sync{};
    next_sync.sequence_id = 1U;
    next_sync.precise_origin_timestamp = 1'124'900'000ULL;
    next_sync.sync_ingress_timestamp = 1'125'000'000ULL;
    PtpTimeInfo next_data{};
    next_data.sync_fup_data = next_sync;

    auto result = validator.Process(next_data);

    EXPECT_FALSE(result.status.is_time_jump_past);
    EXPECT_FALSE(result.status.is_time_jump_future);
}

TEST(TimeJumpsValidatorTest, JumpToFutureWithinThresholdIsNotFlagged)
{
    auto mock = std::make_shared<score::time::HighResSteadyClockBackendMock>();

    TimeJumpsValidator validator(score::time::test_utils::ClockTestFactory<score::time::HighResSteadyTime>::Make(mock),
                                 std::chrono::nanoseconds(500'000),
                                 std::chrono::nanoseconds(5'000'000),
                                 2U);

    // Pass synchronized state debouncing
    EXPECT_CALL(*mock, Now())
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(0)}, {}}))
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(6'000'000'000)}, {}}));

    PtpTimeInfo entry_data{};
    entry_data.status.is_synchronized = true;
    std::ignore = validator.Process(entry_data);
    std::ignore = validator.Process(entry_data);

    SyncFupData baseline_sync{};
    baseline_sync.sequence_id = 0U;
    baseline_sync.precise_origin_timestamp = 1'000'000'000ULL;
    baseline_sync.sync_ingress_timestamp = 1'000'000'000ULL;
    PtpTimeInfo baseline_data{};
    baseline_data.sync_fup_data = baseline_sync;
    std::ignore = validator.Process(baseline_data);

    // predicted_origin = last_t1 + t2_diff = 1'000'000'000 + 125'000'000 = 1'125'000'000
    // current_jump_to_future = current_t1 - predicted_origin = 100'000, which is <= the 500'000 threshold
    SyncFupData next_sync{};
    next_sync.sequence_id = 1U;
    next_sync.precise_origin_timestamp = 1'125'100'000ULL;
    next_sync.sync_ingress_timestamp = 1'125'000'000ULL;
    PtpTimeInfo next_data{};
    next_data.sync_fup_data = next_sync;

    auto result = validator.Process(next_data);

    EXPECT_FALSE(result.status.is_time_jump_past);
    EXPECT_FALSE(result.status.is_time_jump_future);
}

TEST(TimeJumpsValidatorTest, StaysInInitialSyncDebouncingWhenThresholdNotElapsed)
{
    auto mock = std::make_shared<score::time::HighResSteadyClockBackendMock>();

    TimeJumpsValidator validator(score::time::test_utils::ClockTestFactory<score::time::HighResSteadyTime>::Make(mock),
                                 std::chrono::nanoseconds(500'000),
                                 std::chrono::nanoseconds(5'000'000),
                                 2U);

    EXPECT_CALL(*mock, Now())
        // Enter kInitialSyncDebouncing
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(0)}, {}}))
        // Debounce threshold (5'000'000 ns) not yet elapsed -> stays in kInitialSyncDebouncing
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(2'000'000)}, {}}))
        // Debounce threshold elapsed -> transitions to kTimeJumpHandling
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds(6'000'000'000)}, {}}));

    PtpTimeInfo entry_data{};
    entry_data.status.is_synchronized = true;
    std::ignore = validator.Process(entry_data);
    std::ignore = validator.Process(entry_data);
    std::ignore = validator.Process(entry_data);

    SyncFupData first_sync{};
    first_sync.sequence_id = 0U;
    PtpTimeInfo first_data{};
    first_data.sync_fup_data = first_sync;

    auto result = validator.Process(first_data);

    EXPECT_FALSE(result.status.is_time_jump_future);
    EXPECT_FALSE(result.status.is_time_jump_past);
}

}  // namespace td
}  // namespace score
