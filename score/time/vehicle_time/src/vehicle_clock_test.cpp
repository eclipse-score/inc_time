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
#include "score/time/clock/src/scoped_clock_override.h"
#include "score/time/vehicle_time/src/vehicle_clock_backend_mock.h"

#include <score/stop_token.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <type_traits>

using ::testing::_;
using ::testing::Return;

namespace score
{
namespace time
{

namespace
{

class SampleVehicleService
{
  public:
    [[nodiscard]] bool CheckAvailable() const noexcept
    {
        return VehicleClock::GetInstance().IsAvailable();
    }

    [[nodiscard]] VehicleTime::Timepoint GetCurrentTime() const noexcept
    {
        return VehicleClock::GetInstance().Now().TimePoint();
    }
};

}  // namespace

TEST(VehicleClockTest, NowReturnsSynchronizedStatusAndTimepoint)
{
    ::testing::Test::RecordProperty("FullyVerifies", "comp_req__vehicle_time__snapshot");
    ::testing::Test::RecordProperty("TestType", "requirements-based");
    ::testing::Test::RecordProperty("DerivationTechnique", "equivalence-classes");
    ::testing::Test::RecordProperty("Description",
                                    "VehicleClock::Now returns a snapshot combining backend timepoint and status.");

    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    VehicleTimeStatus status;
    status.flags = ClockStatus<VehicleTime::StatusFlag>{VehicleTime::StatusFlag::kSynchronized};
    const ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus> snapshot{
        VehicleTime::Timepoint{std::chrono::nanoseconds{42LL}}, status};

    EXPECT_CALL(*mock, Now()).WillOnce(Return(snapshot));

    const auto result = VehicleClock::GetInstance().Now();

    EXPECT_TRUE(result.Status().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
    EXPECT_EQ(result.TimePoint().time_since_epoch(), std::chrono::nanoseconds{42LL});
}

TEST(VehicleClockTest, NowIsReliableReturnsTrueWhenFlagSet)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    VehicleTimeStatus status;
    status.flags = ClockStatus<VehicleTime::StatusFlag>{VehicleTime::StatusFlag::kSynchronized};
    EXPECT_CALL(*mock, Now())
        .WillOnce(Return(ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus>{VehicleTime::Timepoint{}, status}));

    EXPECT_TRUE(VehicleClock::GetInstance().Now().Status().IsReliable());
}

TEST(VehicleClockTest, NowIsReliableReturnsFalseWhenTimeoutSet)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    VehicleTimeStatus status;
    status.flags = ClockStatus<VehicleTime::StatusFlag>{
        {VehicleTime::StatusFlag::kSynchronized, VehicleTime::StatusFlag::kTimeOut}};
    EXPECT_CALL(*mock, Now())
        .WillOnce(Return(ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus>{VehicleTime::Timepoint{}, status}));

    EXPECT_FALSE(VehicleClock::GetInstance().Now().Status().IsReliable());
}

TEST(VehicleClockTest, InitForwardsToBackend)
{
    ::testing::Test::RecordProperty("FullyVerifies", "comp_req__vehicle_time__lifecycle");
    ::testing::Test::RecordProperty("TestType", "requirements-based");
    ::testing::Test::RecordProperty("DerivationTechnique", "requirements-analysis");
    ::testing::Test::RecordProperty("Description", "VehicleClock::Init delegates to the backend Init call.");

    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    EXPECT_CALL(*mock, Init()).WillOnce(Return(true));

    EXPECT_TRUE(VehicleClock::GetInstance().Init());
}

TEST(VehicleClockTest, IsAvailableReturnsTrueWhenBackendReports)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    EXPECT_CALL(*mock, IsAvailable()).WillOnce(Return(true));

    EXPECT_TRUE(VehicleClock::GetInstance().IsAvailable());
}

TEST(VehicleClockTest, IsAvailableReturnsFalseWhenBackendUnavailable)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    EXPECT_CALL(*mock, IsAvailable()).WillOnce(Return(false));

    EXPECT_FALSE(VehicleClock::GetInstance().IsAvailable());
}

TEST(VehicleClockTest, WaitUntilAvailableForwardsTokenAndDeadlineToBackend)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    score::cpp::stop_source source;
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds{100};

    EXPECT_CALL(*mock, WaitUntilAvailable(_, _)).WillOnce(Return(true));

    EXPECT_TRUE(VehicleClock::GetInstance().WaitUntilAvailable(source.get_token(), until));
}

TEST(VehicleClockTest, SubscribeTimeSlaveSyncDataCapturesAndInvokesCallback)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    VehicleTime::TimeSlaveSyncDataReceivedCallback captured_cb;
    EXPECT_CALL(*mock, SetTimeSlaveSyncDataReceivedCallback(_))
        .WillOnce([&captured_cb](VehicleTime::TimeSlaveSyncDataReceivedCallback&& cb) {
            captured_cb = std::move(cb);
        });

    bool invoked{false};
    VehicleClock::GetInstance().Subscribe<TimeSlaveSyncData<VehicleTime>>(
        [&invoked](const TimeSlaveSyncData<VehicleTime>&) {
            invoked = true;
        });

    TimeSlaveSyncData<VehicleTime> data{};
    captured_cb(data);

    EXPECT_TRUE(invoked);
}

TEST(VehicleClockTest, UnsubscribeTimeSlaveSyncDataForwardsToBackend)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    EXPECT_CALL(*mock, UnsetTimeSlaveSyncDataReceivedCallback()).Times(1);

    VehicleClock::GetInstance().Unsubscribe<TimeSlaveSyncData<VehicleTime>>();
}

TEST(VehicleClockTest, SubscribePDelayMeasurementDataCapturesAndInvokesCallback)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    VehicleTime::PDelayMeasurementFinishedCallback captured_cb;
    EXPECT_CALL(*mock, SetPDelayMeasurementFinishedCallback(_))
        .WillOnce([&captured_cb](VehicleTime::PDelayMeasurementFinishedCallback&& cb) {
            captured_cb = std::move(cb);
        });

    bool invoked{false};
    VehicleClock::GetInstance().Subscribe<PDelayMeasurementData<VehicleTime>>(
        [&invoked](const PDelayMeasurementData<VehicleTime>&) {
            invoked = true;
        });

    PDelayMeasurementData<VehicleTime> data{};
    captured_cb(data);

    EXPECT_TRUE(invoked);
}

TEST(VehicleClockTest, UnsubscribePDelayMeasurementDataForwardsToBackend)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    EXPECT_CALL(*mock, UnsetPDelayMeasurementFinishedCallback()).Times(1);

    VehicleClock::GetInstance().Unsubscribe<PDelayMeasurementData<VehicleTime>>();
}

TEST(VehicleClockTest, VehicleTimeStatusFlagValues)
{
    EXPECT_EQ(static_cast<std::uint8_t>(VehicleTime::StatusFlag::kSynchronized), 1U);
    EXPECT_EQ(static_cast<std::uint8_t>(VehicleTime::StatusFlag::kTimeOut), 0U);
    EXPECT_EQ(static_cast<std::uint8_t>(VehicleTime::StatusFlag::kTimeLeapFuture), 3U);
}

TEST(VehicleClockTest, ScopedClockOverrideInjectsMockIntoSut)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    const VehicleTime::Timepoint expected_tp{std::chrono::nanoseconds{999LL}};

    EXPECT_CALL(*mock, IsAvailable()).WillOnce(Return(true));
    EXPECT_CALL(*mock, Now())
        .WillOnce(Return(ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus>{expected_tp, VehicleTimeStatus{}}));

    SampleVehicleService sut;
    EXPECT_TRUE(sut.CheckAvailable());
    EXPECT_EQ(sut.GetCurrentTime(), expected_tp);
}

TEST(VehicleClockTest, ScopedClockOverrideRestoresBackendAfterScope)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    {
        test_utils::ScopedClockOverride<VehicleTime> guard{mock};
        EXPECT_CALL(*mock, IsAvailable()).WillOnce(Return(false));
        EXPECT_FALSE(VehicleClock::GetInstance().IsAvailable());
    }
    auto mock2 = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard2{mock2};
    EXPECT_CALL(*mock2, IsAvailable()).WillOnce(Return(true));
    EXPECT_TRUE(VehicleClock::GetInstance().IsAvailable());
}

TEST(VehicleClockTest, SubscribeVehicleTimeStatusCapturesAndInvokesCallback)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    VehicleTime::StatusChangedCallback captured_cb;
    EXPECT_CALL(*mock, SetStatusChangedCallback(_)).WillOnce([&captured_cb](VehicleTime::StatusChangedCallback&& cb) {
        captured_cb = std::move(cb);
    });

    bool invoked{false};
    VehicleClock::GetInstance().Subscribe<VehicleTimeStatus>([&invoked](const VehicleTimeStatus&) {
        invoked = true;
    });

    VehicleTimeStatus status{};
    captured_cb(status);

    EXPECT_TRUE(invoked);
}

TEST(VehicleClockTest, UnsubscribeVehicleTimeStatusForwardsToBackend)
{
    auto mock = std::make_shared<VehicleClockBackendMock>();
    test_utils::ScopedClockOverride<VehicleTime> guard{mock};

    EXPECT_CALL(*mock, UnsetStatusChangedCallback()).Times(1);

    VehicleClock::GetInstance().Unsubscribe<VehicleTimeStatus>();
}

TEST(VehicleClockTest, GetInstanceWithoutOverrideUsesStubBackend)
{
    const auto clock = VehicleClock::GetInstance();
    EXPECT_FALSE(clock.IsAvailable());
}

TEST(VehicleClockTest, GetInstanceCalledTwice)
{
    const auto clock1 = VehicleClock::GetInstance();
    const auto clock2 = VehicleClock::GetInstance();
    EXPECT_FALSE(clock1.IsAvailable());
    EXPECT_FALSE(clock2.IsAvailable());
}

}  // namespace time
}  // namespace score
