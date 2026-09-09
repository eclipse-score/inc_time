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
#include "score/time/vehicle_time/src/details/td_impl/vehicle_clock_backend_impl.h"

#include "score/time/clock/src/clock_snapshot.h"
#include "score/time/clock/src/no_status.h"
#include "score/time/clock/src/scoped_clock_override.h"
#include "score/time/high_res_steady_time/src/high_res_steady_clock_backend_mock.h"
#include "score/time_daemon/src/ipc/receiver_mock.h"
#include "score/time_daemon/src/ipc/svt/svt_time_info.h"

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <mutex>
#include <optional>
#include <thread>

namespace score
{
namespace time
{
namespace
{

using namespace std::chrono_literals;
using ::testing::Return;

using SvtMock = score::td::ReceiverMock<score::td::svt::TimeBaseSnapshot>;
using SvtSnapshot = score::td::svt::TimeBaseSnapshot;
using SvtStatus = score::td::svt::TimeBaseStatus;
using SvtSyncData = score::td::svt::SyncFupSnapshot;
using SvtPDelayData = score::td::svt::PDelayDataSnapshot;

constexpr SvtStatus kSynchronizedStatus{true, false, false, false, true};
constexpr SvtStatus kTimeoutStatus{true, true, false, false, true};
constexpr std::chrono::milliseconds kPollInterval{1};
constexpr std::chrono::seconds kWaitTimeout{5};

SvtSnapshot MakeFrame(const SvtStatus status, const double rate_deviation = 0.0) noexcept
{
    return SvtSnapshot{1000ULL, 0ULL, rate_deviation, status, {}, {}};
}

/// @brief Thread-safe frame supplier for the mocked receiver; counts how often the worker polled.
class FrameSource
{
  public:
    void Set(const std::optional<SvtSnapshot>& frame) noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        frame_ = frame;
    }

    std::optional<SvtSnapshot> Get() noexcept
    {
        // Notify under the lock so the object can be destroyed as soon as a waiter resumes.
        const std::lock_guard<std::mutex> lock{mutex_};
        ++polls_;
        polled_.notify_all();
        return frame_;
    }

    std::size_t Polls() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        return polls_;
    }

    /// @brief Blocks until the worker polled at least @p additional more times than right now.
    bool WaitForAdditionalPolls(const std::size_t additional) noexcept
    {
        std::unique_lock<std::mutex> lock{mutex_};
        const std::size_t target = polls_ + additional;
        return polled_.wait_for(lock, kWaitTimeout, [this, target]() noexcept {
            return polls_ >= target;
        });
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable polled_;
    std::optional<SvtSnapshot> frame_{};
    std::size_t polls_{0U};
};

/// @brief Records callback invocations so the test thread can wait for them.
template <typename Event>
class Recorder
{
  public:
    void Record(const Event& event) noexcept
    {
        // Notify under the lock so the recorder can be destroyed as soon as a waiter resumes.
        const std::lock_guard<std::mutex> lock{mutex_};
        last_ = event;
        ++count_;
        recorded_.notify_all();
    }

    bool WaitForCount(const std::size_t count) noexcept
    {
        std::unique_lock<std::mutex> lock{mutex_};
        return recorded_.wait_for(lock, kWaitTimeout, [this, count]() noexcept {
            return count_ >= count;
        });
    }

    std::size_t Count() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        return count_;
    }

    Event Last() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        return last_;
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable recorded_;
    Event last_{};
    std::size_t count_{0U};
};

class VehicleClockBackendImplTest : public ::testing::Test
{
  protected:
    VehicleClockBackendImplTest()
        : mock_hirs_{std::make_shared<HighResSteadyClockBackendMock>()},
          hirs_guard_{mock_hirs_},
          mock_svt_{std::make_shared<SvtMock>()},
          frame_source_{},
          impl_{std::make_unique<detail::VehicleClockBackendImpl>(mock_svt_,
                                                                  HighResSteadyClock::GetInstance(),
                                                                  kPollInterval)}
    {
    }

    void InitBackend()
    {
        EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
        EXPECT_TRUE(impl_->Init());
    }

    void ServeFramesFromSource()
    {
        EXPECT_CALL(*mock_svt_, Receive()).WillRepeatedly([this]() {
            return frame_source_.Get();
        });
    }

    std::shared_ptr<HighResSteadyClockBackendMock> mock_hirs_;
    test_utils::ScopedClockOverride<HighResSteadyTime> hirs_guard_;
    std::shared_ptr<SvtMock> mock_svt_;
    FrameSource frame_source_;
    std::unique_ptr<detail::VehicleClockBackendImpl> impl_;
};

TEST_F(VehicleClockBackendImplTest, IsAvailableReturnsFalseBeforeInit)
{
    EXPECT_FALSE(impl_->IsAvailable());
}

TEST_F(VehicleClockBackendImplTest, InitReturnsFalseWhenReceiverInitFails)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(false));
    EXPECT_FALSE(impl_->Init());
    EXPECT_FALSE(impl_->IsAvailable());
}

TEST_F(VehicleClockBackendImplTest, InitReturnsTrueWhenReceiverInitSucceeds)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    EXPECT_TRUE(impl_->Init());
    EXPECT_TRUE(impl_->IsAvailable());
}

TEST_F(VehicleClockBackendImplTest, InitIsIdempotentAfterSuccess)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    EXPECT_TRUE(impl_->Init());
    EXPECT_TRUE(impl_->Init());
    EXPECT_TRUE(impl_->IsAvailable());
}

TEST_F(VehicleClockBackendImplTest, InitTrueAllowsNowToReturnData)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();
    EXPECT_TRUE(impl_->IsAvailable());

    const SvtSnapshot data{1000ULL, 0ULL, 0.0, {true, false, false, false, true}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(
            ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{HighResSteadyTime::Timepoint{0ns}, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_TRUE(snapshot.Status().IsConsistent());
}

TEST_F(VehicleClockBackendImplTest, NowReturnsInconsistentStatusWhenNotReady)
{
    const auto snapshot = impl_->Now();
    EXPECT_FALSE(snapshot.Status().IsConsistent());
}

TEST_F(VehicleClockBackendImplTest, NowReturnsInconsistentStatusWhenReceiveReturnsNullopt)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(std::nullopt));
    const auto snapshot = impl_->Now();
    EXPECT_FALSE(snapshot.Status().IsConsistent());
}

TEST_F(VehicleClockBackendImplTest, NowReturnsInconsistentStatusWhenLocalClockBehindCapture)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    const SvtSnapshot data{1000ULL, 600ULL, 0.0, {true, false, false, false, true}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));

    const HighResSteadyTime::Timepoint local_now{500ns};
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{local_now, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_FALSE(snapshot.Status().IsConsistent());
}

TEST_F(VehicleClockBackendImplTest, NowComputesAdjustedTimestamp)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    // adjusted = ptp_at_capture + (now_local - local_at_capture)
    //          = 1000 + (600 - 500) = 1100 ns
    const SvtSnapshot data{1000ULL, 500ULL, 0.0, {true, false, false, false, true}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));

    const HighResSteadyTime::Timepoint local_now{600ns};
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{local_now, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_EQ(snapshot.TimePoint().time_since_epoch().count(), 1100);
}

TEST_F(VehicleClockBackendImplTest, NowSetsSynchronizedFlagFromSvtStatus)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    const SvtSnapshot data{0ULL, 0ULL, 0.0, {true, false, false, false, true}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(
            ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{HighResSteadyTime::Timepoint{0ns}, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_TRUE(snapshot.Status().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
}

TEST_F(VehicleClockBackendImplTest, NowSetsTimeOutFlagFromSvtStatus)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    const SvtSnapshot data{0ULL, 0ULL, 0.0, {false, true, false, false, true}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(
            ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{HighResSteadyTime::Timepoint{0ns}, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_TRUE(snapshot.Status().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));
}

TEST_F(VehicleClockBackendImplTest, NowSetsTimeLeapFutureFlagFromSvtStatus)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    const SvtSnapshot data{0ULL, 0ULL, 0.0, {false, false, true, false, true}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(
            ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{HighResSteadyTime::Timepoint{0ns}, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_TRUE(snapshot.Status().IsFlagActive(VehicleTime::StatusFlag::kTimeLeapFuture));
}

TEST_F(VehicleClockBackendImplTest, NowSetsTimeLeapPastFlagFromSvtStatus)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    const SvtSnapshot data{0ULL, 0ULL, 0.0, {false, false, false, true, true}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(
            ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{HighResSteadyTime::Timepoint{0ns}, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_TRUE(snapshot.Status().IsFlagActive(VehicleTime::StatusFlag::kTimeLeapPast));
}

TEST_F(VehicleClockBackendImplTest, NowReturnsInconsistentStatusWhenIsCorrectIsFalse)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    const SvtSnapshot data{0ULL, 0ULL, 0.0, {false, false, false, false, false}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(
            ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{HighResSteadyTime::Timepoint{0ns}, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_FALSE(snapshot.Status().IsConsistent());
}

TEST_F(VehicleClockBackendImplTest, NowForwardsRateDeviation)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();

    const SvtSnapshot data{0ULL, 0ULL, 2.5, {false, false, false, false, true}, {}, {}};
    EXPECT_CALL(*mock_svt_, Receive()).WillOnce(Return(data));
    EXPECT_CALL(*mock_hirs_, Now())
        .WillOnce(Return(
            ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{HighResSteadyTime::Timepoint{0ns}, NoStatus{}}));

    const auto snapshot = impl_->Now();
    EXPECT_DOUBLE_EQ(snapshot.Status().RateDeviation(), 2.5);
}

TEST_F(VehicleClockBackendImplTest, WaitUntilAvailableReturnsTrueWhenInitSucceeds)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(true));
    impl_->Init();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    EXPECT_TRUE(impl_->WaitUntilAvailable(score::cpp::stop_source{}.get_token(), deadline));
}

TEST_F(VehicleClockBackendImplTest, WaitUntilAvailableReturnsFalseWhenStopRequested)
{
    score::cpp::stop_source ss;
    ss.request_stop();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    EXPECT_FALSE(impl_->WaitUntilAvailable(ss.get_token(), deadline));
}

TEST_F(VehicleClockBackendImplTest, WaitUntilAvailableReturnsFalseWhenDeadlinePassed)
{
    const auto past_deadline = std::chrono::steady_clock::now() - std::chrono::seconds{1};
    EXPECT_FALSE(impl_->WaitUntilAvailable(score::cpp::stop_source{}.get_token(), past_deadline));
}

// ---------------------------------------------------------------------------
// Callback delivery — worker lifecycle
// ---------------------------------------------------------------------------

TEST_F(VehicleClockBackendImplTest, WorkerDoesNotPollWhileNoCallbackIsRegistered)
{
    InitBackend();
    EXPECT_CALL(*mock_svt_, Receive()).Times(0);

    std::this_thread::sleep_for(20 * kPollInterval);
}

TEST_F(VehicleClockBackendImplTest, CallbacksAreNotDeliveredBeforeInit)
{
    EXPECT_CALL(*mock_svt_, Receive()).Times(0);

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    std::this_thread::sleep_for(20 * kPollInterval);
    EXPECT_EQ(recorder.Count(), 0U);
}

TEST_F(VehicleClockBackendImplTest, CallbacksAreNotDeliveredWhenInitFails)
{
    EXPECT_CALL(*mock_svt_, Init()).WillOnce(Return(false));
    EXPECT_CALL(*mock_svt_, Receive()).Times(0);
    EXPECT_FALSE(impl_->Init());

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    std::this_thread::sleep_for(20 * kPollInterval);
    EXPECT_EQ(recorder.Count(), 0U);
}

TEST_F(VehicleClockBackendImplTest, CallbackRegisteredBeforeInitIsDeliveredOnceInitSucceeds)
{
    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    frame_source_.Set(MakeFrame(kSynchronizedStatus));
    ServeFramesFromSource();
    InitBackend();

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
}

TEST_F(VehicleClockBackendImplTest, NoCallbackIsDeliveredWhileReceiveReturnsNullopt)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(std::nullopt);

    Recorder<VehicleTimeStatus> status_recorder;
    Recorder<TimeSlaveSyncData<VehicleTime>> sync_recorder;
    Recorder<PDelayMeasurementData<VehicleTime>> pdelay_recorder;
    impl_->SetStatusChangedCallback([&status_recorder](const VehicleTimeStatus& status) {
        status_recorder.Record(status);
    });
    impl_->SetTimeSlaveSyncDataReceivedCallback([&sync_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        sync_recorder.Record(data);
    });
    impl_->SetPDelayMeasurementFinishedCallback([&pdelay_recorder](const PDelayMeasurementData<VehicleTime>& data) {
        pdelay_recorder.Record(data);
    });

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(status_recorder.Count(), 0U);
    EXPECT_EQ(sync_recorder.Count(), 0U);
    EXPECT_EQ(pdelay_recorder.Count(), 0U);
}

TEST_F(VehicleClockBackendImplTest, DestructorJoinsWorkerWhileCallbacksAreRegistered)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    impl_.reset();

    const auto polls_after_destruction = frame_source_.Polls();
    std::this_thread::sleep_for(20 * kPollInterval);
    EXPECT_EQ(frame_source_.Polls(), polls_after_destruction);
}

// ---------------------------------------------------------------------------
// Callback delivery — VehicleTimeStatus
// ---------------------------------------------------------------------------

TEST_F(VehicleClockBackendImplTest, StatusCallbackFiresOnFirstFrameAfterRegistration)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus, 2.5));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
    EXPECT_FALSE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));
    EXPECT_DOUBLE_EQ(recorder.Last().RateDeviation(), 2.5);
}

TEST_F(VehicleClockBackendImplTest, StatusCallbackFiresForInvalidFrameWithEmptyFlags)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(SvtStatus{true, false, false, false, false}));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_FALSE(recorder.Last().IsConsistent());
}

TEST_F(VehicleClockBackendImplTest, StatusCallbackDoesNotRepeatWhileFlagsAreUnchanged)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

TEST_F(VehicleClockBackendImplTest, StatusCallbackFiresWhenFlagsChange)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    frame_source_.Set(MakeFrame(kTimeoutStatus));

    ASSERT_TRUE(recorder.WaitForCount(2U));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
}

TEST_F(VehicleClockBackendImplTest, StatusCallbackIgnoresRateDeviationChanges)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus, 1.0));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    frame_source_.Set(MakeFrame(kSynchronizedStatus, 9.0));

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
    EXPECT_DOUBLE_EQ(recorder.Last().RateDeviation(), 1.0);
}

TEST_F(VehicleClockBackendImplTest, StatusCallbackReRegisteredReceivesUnchangedStatusAgain)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> first_recorder;
    impl_->SetStatusChangedCallback([&first_recorder](const VehicleTimeStatus& status) {
        first_recorder.Record(status);
    });
    ASSERT_TRUE(first_recorder.WaitForCount(1U));

    impl_->UnsetStatusChangedCallback();

    Recorder<VehicleTimeStatus> second_recorder;
    impl_->SetStatusChangedCallback([&second_recorder](const VehicleTimeStatus& status) {
        second_recorder.Record(status);
    });

    ASSERT_TRUE(second_recorder.WaitForCount(1U));
    EXPECT_TRUE(second_recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
    EXPECT_EQ(first_recorder.Count(), 1U);

    // Afterwards only changes are delivered.
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(second_recorder.Count(), 1U);
}

TEST_F(VehicleClockBackendImplTest, StatusCallbackReplacedWithoutUnsetReceivesUnchangedStatusAgain)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> first_recorder;
    impl_->SetStatusChangedCallback([&first_recorder](const VehicleTimeStatus& status) {
        first_recorder.Record(status);
    });
    ASSERT_TRUE(first_recorder.WaitForCount(1U));

    Recorder<VehicleTimeStatus> second_recorder;
    impl_->SetStatusChangedCallback([&second_recorder](const VehicleTimeStatus& status) {
        second_recorder.Record(status);
    });

    ASSERT_TRUE(second_recorder.WaitForCount(1U));
    EXPECT_TRUE(second_recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
    EXPECT_EQ(first_recorder.Count(), 1U);
}

TEST_F(VehicleClockBackendImplTest, StatusCallbackRegisteredWhileWorkerIsActiveReceivesCurrentStatusFirst)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kTimeoutStatus));

    // Worker already running and has seen the timeout status through another callback.
    impl_->SetPDelayMeasurementFinishedCallback([](const PDelayMeasurementData<VehicleTime>&) {});
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));

    frame_source_.Set(MakeFrame(kSynchronizedStatus));
    ASSERT_TRUE(recorder.WaitForCount(2U));
    EXPECT_FALSE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));
}

TEST_F(VehicleClockBackendImplTest, UnsetStatusCallbackStopsDelivery)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    impl_->UnsetStatusChangedCallback();
    frame_source_.Set(MakeFrame(kTimeoutStatus));

    // Keep the worker polling through another registered callback and verify status stays silent.
    impl_->SetPDelayMeasurementFinishedCallback([](const PDelayMeasurementData<VehicleTime>&) {});
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

// ---------------------------------------------------------------------------
// Callback delivery — TimeSlaveSyncData
// ---------------------------------------------------------------------------

TEST_F(VehicleClockBackendImplTest, SyncDataCallbackReceivesCurrentFrameOnRegistration)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data.sequence_id = 1U;
    frame_source_.Set(frame);

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    impl_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        recorder.Record(data);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_EQ(recorder.Last().sequence_id, 1U);

    // Unchanged frame: no further delivery.
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

TEST_F(VehicleClockBackendImplTest, SyncDataCallbackFiresOnEachNewFrame)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data.sequence_id = 1U;
    frame_source_.Set(frame);

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    impl_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        recorder.Record(data);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    frame.sync_fup_data.sequence_id = 2U;
    frame_source_.Set(frame);
    ASSERT_TRUE(recorder.WaitForCount(2U));
    EXPECT_EQ(recorder.Last().sequence_id, 2U);

    // Unchanged frame: no further delivery.
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 2U);

    frame.sync_fup_data.sequence_id = 3U;
    frame_source_.Set(frame);
    ASSERT_TRUE(recorder.WaitForCount(3U));
    EXPECT_EQ(recorder.Last().sequence_id, 3U);
}

TEST_F(VehicleClockBackendImplTest, SyncDataCallbackRegisteredWhileWorkerIsActiveReceivesCurrentFrameFirst)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data.sequence_id = 1U;
    frame_source_.Set(frame);

    // Worker already running and has seen sequence 1 through another callback.
    impl_->SetStatusChangedCallback([](const VehicleTimeStatus&) {});
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    impl_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        recorder.Record(data);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_EQ(recorder.Last().sequence_id, 1U);

    frame.sync_fup_data.sequence_id = 2U;
    frame_source_.Set(frame);
    ASSERT_TRUE(recorder.WaitForCount(2U));
    EXPECT_EQ(recorder.Last().sequence_id, 2U);
}

TEST_F(VehicleClockBackendImplTest, SyncDataCallbackReplacedWithoutUnsetReceivesCurrentFrameFirst)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data.sequence_id = 1U;
    frame_source_.Set(frame);

    Recorder<TimeSlaveSyncData<VehicleTime>> first_recorder;
    impl_->SetTimeSlaveSyncDataReceivedCallback([&first_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        first_recorder.Record(data);
    });
    ASSERT_TRUE(first_recorder.WaitForCount(1U));

    Recorder<TimeSlaveSyncData<VehicleTime>> second_recorder;
    impl_->SetTimeSlaveSyncDataReceivedCallback([&second_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        second_recorder.Record(data);
    });

    ASSERT_TRUE(second_recorder.WaitForCount(1U));
    EXPECT_EQ(second_recorder.Last().sequence_id, 1U);
    EXPECT_EQ(first_recorder.Count(), 1U);
}

TEST_F(VehicleClockBackendImplTest, SyncDataCallbackReceivesConvertedFields)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    impl_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        recorder.Record(data);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data = SvtSyncData{101ULL, 202ULL, 303ULL, 404ULL, 0x10000ULL, 55U, 606ULL, 7U, 0xABCDULL};
    frame_source_.Set(frame);

    ASSERT_TRUE(recorder.WaitForCount(2U));
    const auto data = recorder.Last();
    EXPECT_EQ(data.precise_origin_timestamp.time_since_epoch(), 101ns);
    EXPECT_EQ(data.reference_global_timestamp.time_since_epoch(), 202ns);
    EXPECT_EQ(data.reference_local_timestamp.time_since_epoch(), 303ns);
    EXPECT_EQ(data.sync_ingress_timestamp.time_since_epoch(), 404ns);
    EXPECT_EQ(data.correction_field, 0x10000LL);
    EXPECT_EQ(data.sequence_id, 55U);
    EXPECT_EQ(data.pdelay, 606ns);
    EXPECT_EQ(data.source_port_identity.port_number, 7U);
    EXPECT_EQ(data.source_port_identity.clock_identity, 0xABCDULL);
}

TEST_F(VehicleClockBackendImplTest, UnsetSyncDataCallbackStopsDelivery)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame_source_.Set(frame);

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    impl_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        recorder.Record(data);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));
    impl_->UnsetTimeSlaveSyncDataReceivedCallback();

    // Keep the worker polling through another registered callback.
    impl_->SetStatusChangedCallback([](const VehicleTimeStatus&) {});
    frame.sync_fup_data.sequence_id = 9U;
    frame_source_.Set(frame);

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

// ---------------------------------------------------------------------------
// Callback delivery — PDelayMeasurementData
// ---------------------------------------------------------------------------

TEST_F(VehicleClockBackendImplTest, PDelayCallbackFiresOnEachNewFrame)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.pdelay_data.sequence_id = 1U;
    frame_source_.Set(frame);

    Recorder<PDelayMeasurementData<VehicleTime>> recorder;
    impl_->SetPDelayMeasurementFinishedCallback([&recorder](const PDelayMeasurementData<VehicleTime>& data) {
        recorder.Record(data);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_EQ(recorder.Last().sequence_id, 1U);

    frame.pdelay_data.sequence_id = 2U;
    frame_source_.Set(frame);
    ASSERT_TRUE(recorder.WaitForCount(2U));
    EXPECT_EQ(recorder.Last().sequence_id, 2U);

    // Unchanged frame: no further delivery.
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 2U);

    frame.pdelay_data.sequence_id = 3U;
    frame_source_.Set(frame);
    ASSERT_TRUE(recorder.WaitForCount(3U));
    EXPECT_EQ(recorder.Last().sequence_id, 3U);
}

TEST_F(VehicleClockBackendImplTest, PDelayCallbackReceivesConvertedFields)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<PDelayMeasurementData<VehicleTime>> recorder;
    impl_->SetPDelayMeasurementFinishedCallback([&recorder](const PDelayMeasurementData<VehicleTime>& data) {
        recorder.Record(data);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    auto frame = MakeFrame(kSynchronizedStatus);
    frame.pdelay_data =
        SvtPDelayData{11ULL, 22ULL, 33ULL, 44ULL, 55ULL, 66ULL, 77U, 88ULL, 3U, 0x1111ULL, 4U, 0x2222ULL};
    frame_source_.Set(frame);

    ASSERT_TRUE(recorder.WaitForCount(2U));
    const auto data = recorder.Last();
    EXPECT_EQ(data.request_origin_timestamp.time_since_epoch(), 11ns);
    EXPECT_EQ(data.request_receipt_timestamp.time_since_epoch(), 22ns);
    EXPECT_EQ(data.response_origin_timestamp.time_since_epoch(), 33ns);
    EXPECT_EQ(data.response_receipt_timestamp.time_since_epoch(), 44ns);
    EXPECT_EQ(data.reference_global_timestamp.time_since_epoch(), 55ns);
    EXPECT_EQ(data.reference_local_timestamp.time_since_epoch(), 66ns);
    EXPECT_EQ(data.sequence_id, 77U);
    EXPECT_EQ(data.pdelay, 88ns);
    EXPECT_EQ(data.request_port_identity.port_number, 3U);
    EXPECT_EQ(data.request_port_identity.clock_identity, 0x1111ULL);
    EXPECT_EQ(data.response_port_identity.port_number, 4U);
    EXPECT_EQ(data.response_port_identity.clock_identity, 0x2222ULL);
}

TEST_F(VehicleClockBackendImplTest, UnsetPDelayCallbackStopsDelivery)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame_source_.Set(frame);

    Recorder<PDelayMeasurementData<VehicleTime>> recorder;
    impl_->SetPDelayMeasurementFinishedCallback([&recorder](const PDelayMeasurementData<VehicleTime>& data) {
        recorder.Record(data);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));
    impl_->UnsetPDelayMeasurementFinishedCallback();

    // Keep the worker polling through another registered callback.
    impl_->SetStatusChangedCallback([](const VehicleTimeStatus&) {});
    frame.pdelay_data.sequence_id = 9U;
    frame_source_.Set(frame);

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

TEST_F(VehicleClockBackendImplTest, AllThreeCallbacksAreDeliveredFromTheSameFrame)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame_source_.Set(frame);

    Recorder<VehicleTimeStatus> status_recorder;
    Recorder<TimeSlaveSyncData<VehicleTime>> sync_recorder;
    Recorder<PDelayMeasurementData<VehicleTime>> pdelay_recorder;
    impl_->SetTimeSlaveSyncDataReceivedCallback([&sync_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        sync_recorder.Record(data);
    });
    impl_->SetPDelayMeasurementFinishedCallback([&pdelay_recorder](const PDelayMeasurementData<VehicleTime>& data) {
        pdelay_recorder.Record(data);
    });
    impl_->SetStatusChangedCallback([&status_recorder](const VehicleTimeStatus& status) {
        status_recorder.Record(status);
    });
    ASSERT_TRUE(status_recorder.WaitForCount(1U));
    ASSERT_TRUE(sync_recorder.WaitForCount(1U));
    ASSERT_TRUE(pdelay_recorder.WaitForCount(1U));

    frame.status = kTimeoutStatus;
    frame.sync_fup_data.sequence_id = 5U;
    frame.pdelay_data.sequence_id = 6U;
    frame_source_.Set(frame);

    ASSERT_TRUE(status_recorder.WaitForCount(2U));
    ASSERT_TRUE(sync_recorder.WaitForCount(2U));
    ASSERT_TRUE(pdelay_recorder.WaitForCount(2U));
    EXPECT_TRUE(status_recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));
    EXPECT_EQ(sync_recorder.Last().sequence_id, 5U);
    EXPECT_EQ(pdelay_recorder.Last().sequence_id, 6U);
}

// ---------------------------------------------------------------------------
// Callback delivery — thread safety of Subscribe / Unsubscribe
// ---------------------------------------------------------------------------

TEST_F(VehicleClockBackendImplTest, UnsetFromWithinCallbackDoesNotDeadlock)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    impl_->SetStatusChangedCallback([this, &recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
        impl_->UnsetStatusChangedCallback();
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    // Keep the worker polling through another registered callback; the status callback must stay silent.
    impl_->SetPDelayMeasurementFinishedCallback([](const PDelayMeasurementData<VehicleTime>&) {});
    frame_source_.Set(MakeFrame(kTimeoutStatus));

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

TEST_F(VehicleClockBackendImplTest, SubscribingToAnotherEventFromWithinCallbackDoesNotDeadlock)
{
    InitBackend();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame_source_.Set(frame);

    Recorder<VehicleTimeStatus> status_recorder;
    Recorder<TimeSlaveSyncData<VehicleTime>> sync_recorder;
    impl_->SetStatusChangedCallback([this, &status_recorder, &sync_recorder](const VehicleTimeStatus& status) {
        status_recorder.Record(status);
        impl_->SetTimeSlaveSyncDataReceivedCallback([&sync_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
            sync_recorder.Record(data);
        });
    });
    ASSERT_TRUE(status_recorder.WaitForCount(1U));
    ASSERT_TRUE(sync_recorder.WaitForCount(1U));

    frame.sync_fup_data.sequence_id = 42U;
    frame_source_.Set(frame);

    ASSERT_TRUE(sync_recorder.WaitForCount(2U));
    EXPECT_EQ(sync_recorder.Last().sequence_id, 42U);
}

TEST_F(VehicleClockBackendImplTest, UnsetBlocksUntilInFlightCallbackReturns)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    std::promise<void> callback_entered;
    std::promise<void> release_callback;
    auto release_future = release_callback.get_future().share();
    impl_->SetStatusChangedCallback([&callback_entered, release_future](const VehicleTimeStatus&) {
        callback_entered.set_value();
        release_future.wait();
    });
    ASSERT_EQ(callback_entered.get_future().wait_for(kWaitTimeout), std::future_status::ready);

    auto unset_done = std::async(std::launch::async, [this]() {
        impl_->UnsetStatusChangedCallback();
    });
    EXPECT_EQ(unset_done.wait_for(50 * kPollInterval), std::future_status::timeout);

    release_callback.set_value();
    EXPECT_EQ(unset_done.wait_for(kWaitTimeout), std::future_status::ready);
}

TEST_F(VehicleClockBackendImplTest, SettingEmptyCallbackDoesNotInvokeAnything)
{
    InitBackend();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    impl_->SetStatusChangedCallback(VehicleTime::StatusChangedCallback{});
    impl_->SetTimeSlaveSyncDataReceivedCallback(VehicleTime::TimeSlaveSyncDataReceivedCallback{});
    impl_->SetPDelayMeasurementFinishedCallback(VehicleTime::PDelayMeasurementFinishedCallback{});

    // No callback is registered, so the worker must not poll at all.
    std::this_thread::sleep_for(20 * kPollInterval);
    EXPECT_EQ(frame_source_.Polls(), 0U);
}

}  // namespace
}  // namespace time
}  // namespace score
