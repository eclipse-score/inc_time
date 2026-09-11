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
#include "score/time/clock/src/clock_test_factory.h"
#include "score/time/clock/src/scoped_clock_override.h"
#include "score/time/system_time/src/system_clock_backend_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <type_traits>

using ::testing::Return;

namespace score
{
namespace time
{

namespace
{

class SampleSystemService
{
  public:
    [[nodiscard]] std::chrono::system_clock::time_point GetCurrentTime() const noexcept
    {
        return SystemClock::GetInstance().Now().TimePoint();
    }
};

}  // namespace

TEST(SystemClockTest, NowReturnsTimepointSuitableForDurationArithmetic)
{
    ::testing::Test::RecordProperty("FullyVerifies", "comp_req__system_time__snapshot");
    ::testing::Test::RecordProperty("TestType", "requirements-based");
    ::testing::Test::RecordProperty("DerivationTechnique", "requirements-analysis");
    ::testing::Test::RecordProperty("Description",
                                    "SystemClock::Now returns a snapshot backed by std::chrono::system_clock.");

    auto mock = std::make_shared<SystemClockBackendMock>();
    test_utils::ScopedClockOverride<std::chrono::system_clock> guard{mock};

    const std::chrono::system_clock::time_point tp{
        std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds{1'000'000LL})};
    EXPECT_CALL(*mock, Now())
        .WillOnce(Return(ClockSnapshot<std::chrono::system_clock::time_point, NoStatus>{tp, NoStatus{}}));

    const auto result = SystemClock::GetInstance().Now();
    const auto deadline = result.TimePoint() + std::chrono::seconds{5};

    EXPECT_EQ(deadline.time_since_epoch(), std::chrono::nanoseconds{1'000'000LL} + std::chrono::seconds{5});
}

TEST(SystemClockTest, NowReturnsExactTimepointFromMock)
{
    auto mock = std::make_shared<SystemClockBackendMock>();
    test_utils::ScopedClockOverride<std::chrono::system_clock> guard{mock};

    const std::chrono::system_clock::time_point tp{std::chrono::seconds{42}};
    EXPECT_CALL(*mock, Now())
        .WillOnce(Return(ClockSnapshot<std::chrono::system_clock::time_point, NoStatus>{tp, NoStatus{}}));

    EXPECT_EQ(SystemClock::GetInstance().Now().TimePoint(), tp);
}

TEST(SystemClockTest, NowSnapshotCarriesNoStatus)
{
    auto mock = std::make_shared<SystemClockBackendMock>();
    test_utils::ScopedClockOverride<std::chrono::system_clock> guard{mock};

    EXPECT_CALL(*mock, Now())
        .WillOnce(Return(ClockSnapshot<std::chrono::system_clock::time_point, NoStatus>{
            std::chrono::system_clock::time_point{}, NoStatus{}}));

    const auto result = SystemClock::GetInstance().Now();
    const NoStatus status = result.Status();
    (void)status;
    SUCCEED();
}

TEST(SystemClockTest, ScopedClockOverrideInjectsMockIntoSut)
{
    auto mock = std::make_shared<SystemClockBackendMock>();
    test_utils::ScopedClockOverride<std::chrono::system_clock> guard{mock};

    const std::chrono::system_clock::time_point expected{
        std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::nanoseconds{999'000LL})};
    EXPECT_CALL(*mock, Now())
        .WillOnce(Return(ClockSnapshot<std::chrono::system_clock::time_point, NoStatus>{expected, NoStatus{}}));

    SampleSystemService sut;
    EXPECT_EQ(sut.GetCurrentTime(), expected);
}

TEST(SystemClockTest, ScopedClockOverrideRestoresBackendAfterScope)
{
    auto mock = std::make_shared<SystemClockBackendMock>();
    {
        test_utils::ScopedClockOverride<std::chrono::system_clock> guard{mock};
        const std::chrono::system_clock::time_point tp{std::chrono::seconds{1}};
        EXPECT_CALL(*mock, Now())
            .WillOnce(Return(ClockSnapshot<std::chrono::system_clock::time_point, NoStatus>{tp, NoStatus{}}));
        EXPECT_EQ(SystemClock::GetInstance().Now().TimePoint(), tp);
    }
    auto mock2 = std::make_shared<SystemClockBackendMock>();
    test_utils::ScopedClockOverride<std::chrono::system_clock> guard2{mock2};
    const std::chrono::system_clock::time_point tp2{std::chrono::seconds{2}};
    EXPECT_CALL(*mock2, Now())
        .WillOnce(Return(ClockSnapshot<std::chrono::system_clock::time_point, NoStatus>{tp2, NoStatus{}}));
    EXPECT_EQ(SystemClock::GetInstance().Now().TimePoint(), tp2);
}

TEST(SystemClockTest, GetInstanceWithoutOverrideUsesStubBackend)
{
    const auto clock = SystemClock::GetInstance();
    EXPECT_EQ(clock.Now().TimePoint().time_since_epoch().count(), 0);
}

TEST(SystemClockTest, GetInstanceCalledTwice)
{
    const auto clock1 = SystemClock::GetInstance();
    const auto clock2 = SystemClock::GetInstance();
    EXPECT_EQ(clock1.Now().TimePoint().time_since_epoch().count(), 0);
    EXPECT_EQ(clock2.Now().TimePoint().time_since_epoch().count(), 0);
}

}  // namespace time
}  // namespace score
