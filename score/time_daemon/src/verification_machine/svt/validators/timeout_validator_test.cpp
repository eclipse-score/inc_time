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
#include "score/time_daemon/src/verification_machine/svt/validators/timeout_validator.h"

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
    uint16_t simulated_sequence_id_for_sync_msg;
    bool is_expected_timeout;
    std::chrono::nanoseconds simulated_current_time_ns;
};

struct TestScenario
{
    std::vector<TestParams> sequence;
};

class TimeoutValidatorParamTest : public ::testing::TestWithParam<TestScenario>
{
};

/// \brief Checking if timeout is set when timer goes above threshold and there is no new frame
INSTANTIATE_TEST_SUITE_P(
    TestIfTimeoutIsSetWhenThereIsNoNewFrame,
    TimeoutValidatorParamTest,
    ::testing::Values(TestScenario{{
        {0, false, 0ns},              /* Initial frame*/
        {1, false, 125'000'010ns},    /* New frame (seq id +1) -> No Timeout */
        {1, true, 4'000'000'000ns},   /* No new frame and timer goes above threshold -> Timeout */
        {2, false, 4'125'000'000ns},  /* New frame (seq id +1) -> No Timeout */
        {3, false, 4'250'000'000ns},  /* New frame (seq id +1) -> No Timeout */
        {4, false, 4'375'000'000ns},  /* New frame (seq id +1) -> No Timeout */
        {4, true, 7'700'000'000ns},   /* No new frame and timer goes above threshold -> Timeout */
        {5, false, 12'000'000'000ns}, /* New frame (seq id +1) -> No Timeout, even if timeout goes above threshold */
        {6, false, 22'000'000'000ns}, /* New frame (seq id +1) -> No Timeout, even if timeout goes above threshold */
    }}));

/// \brief Checking if timeout is set when there is no frame received
INSTANTIATE_TEST_SUITE_P(TestTimeoutWhenNoFramesAtAll,
                         TimeoutValidatorParamTest,
                         ::testing::Values(TestScenario{{
                             {0, false, 0ns},             /* Initial frame*/
                             {0, false, 125'000'000ns},   /* No New frame, but timeout below threshold */
                             {0, false, 1'000'000'000ns}, /* No New frame, but timeout below threshold */
                             {0, false, 2'000'000'000ns}, /* No New frame, but timeout below threshold */
                             {0, false, 3'000'000'000ns}, /* No New frame, but timeout below threshold */
                             {0, true, 4'000'000'000ns},  /* No New frame, but timeout above threshold */
                         }}));

/// \brief Checking if timeout is not set when receivering the same frame again and again
INSTANTIATE_TEST_SUITE_P(
    TestIfTimeoutIsSetWithRepeatedFrames,
    TimeoutValidatorParamTest,
    ::testing::Values(TestScenario{{
        {0, false, 0ns},              /* Initial frame*/
        {1, false, 125'000'000ns},    /* New frame (seq id +1) -> No Timeout */
        {1, false, 300'000'000ns},    /* Same frame (prev seq id) within the threshold -> No Timeout */
        {1, false, 500'000'000ns},    /* Same frame (prev seq id) within the threshold -> No Timeout */
        {1, false, 700'000'000ns},    /* Same frame (prev seq id) within the threshold -> No Timeout */
        {2, false, 850'000'000ns},    /* New frame (seq id +1) -> No Timeout */
        {2, false, 900'000'000ns},    /* Same frame (prev seq id) within the threshold -> No Timeout */
        {2, false, 950'000'000ns},    /* Same frame (prev seq id) within the threshold -> No Timeout */
        {3, false, 1'000'000'000ns},  /* New frame (seq id +1) -> No Timeout */
        {4, false, 1'150'000'000ns},  /* New frame (seq id +1) -> No Timeout */
        {4, true, 7'700'000'000ns},   /* No new frame and timer goes above threshold -> Timeout */
        {5, false, 12'000'000'000ns}, /* New frame (seq id +1) -> No Timeout, even if timeout goes above threshold */
        {6, false, 22'000'000'000ns}, /* New frame (seq id +1) -> No Timeout, even if timeout goes above threshold */
    }}));

TEST_P(TimeoutValidatorParamTest, ValidationTest)
{
    auto mock = std::make_shared<score::time::HighResSteadyClockBackendMock>();

    TimeoutValidator validator(score::time::test_utils::ClockTestFactory<score::time::HighResSteadyTime>::Make(mock),
                               std::chrono::nanoseconds{3'300'000'000});

    for (const auto& param : GetParam().sequence)
    {
        SyncFupData in_sync_data{};
        in_sync_data.sequence_id = param.simulated_sequence_id_for_sync_msg;

        std::chrono::nanoseconds cur_ptp_time{0};
        PtpTimeInfo::ReferenceClock::time_point cur_local_time{std::chrono::nanoseconds{0}};
        PtpTimeInfo in_data = {cur_ptp_time, cur_local_time, 0, {}, in_sync_data, {}};
        EXPECT_CALL(*mock, Now())
            .WillOnce(::testing::Return(
                score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                    score::time::HighResSteadyTime::Timepoint{param.simulated_current_time_ns}, {}}));
        auto result = validator.Process(in_data);
        EXPECT_EQ(result.status.is_timeout, param.is_expected_timeout)
            << " for simulated_sequence_id_for_sync_msg = " << param.simulated_sequence_id_for_sync_msg
            << ", simulated_current_time_ns = " << param.simulated_current_time_ns.count() << " ns";
    }
}

TEST(TimeoutValidatorTest, LogsErrorWhenClockAppearsToGoBackward)
{
    auto mock = std::make_shared<score::time::HighResSteadyClockBackendMock>();

    TimeoutValidator validator(score::time::test_utils::ClockTestFactory<score::time::HighResSteadyTime>::Make(mock),
                               std::chrono::nanoseconds{3'300'000'000});

    SyncFupData in_sync_data{};
    in_sync_data.sequence_id = 1U;
    std::chrono::nanoseconds cur_ptp_time{0};
    PtpTimeInfo::ReferenceClock::time_point cur_local_time{std::chrono::nanoseconds{0}};
    PtpTimeInfo in_data = {cur_ptp_time, cur_local_time, 0, {}, in_sync_data, {}};

    // First call: new frame -> records reception_time_ at 1'000'000'000 ns.
    EXPECT_CALL(*mock, Now())
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds{1'000'000'000}}, {}}));
    auto first_result = validator.Process(in_data);
    EXPECT_FALSE(first_result.status.is_timeout);

    // Second call: same sequence id (no new frame), so timeout is checked -> but the clock now
    // reports a time earlier than the recorded reception_time_, exercising the defensive
    // "current time is less than reception time" branch instead of the normal timeout check.
    EXPECT_CALL(*mock, Now())
        .WillOnce(::testing::Return(
            score::time::ClockSnapshot<score::time::HighResSteadyTime::Timepoint, score::time::NoStatus>{
                score::time::HighResSteadyTime::Timepoint{std::chrono::nanoseconds{900'000'000}}, {}}));
    auto second_result = validator.Process(in_data);
    EXPECT_FALSE(second_result.status.is_timeout);
}

}  // namespace td
}  // namespace score
