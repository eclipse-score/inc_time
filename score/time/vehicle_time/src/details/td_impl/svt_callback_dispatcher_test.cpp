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
#include "score/time/vehicle_time/src/details/td_impl/svt_callback_dispatcher.h"

#include "score/time/vehicle_time/src/details/td_impl/svt_test_helpers.h"
#include "score/time/vehicle_time/src/vehicle_clock.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <thread>

namespace score
{
namespace time
{
namespace
{

using namespace std::chrono_literals;
using namespace test_helpers;

class SvtCallbackDispatcherTest : public ::testing::Test
{
  protected:
    SvtCallbackDispatcherTest()
        : mock_svt_{std::make_shared<SvtMock>()},
          frame_source_{},
          dispatcher_{std::make_unique<detail::SvtCallbackDispatcher>(mock_svt_, kPollInterval)}
    {
    }

    void ServeFramesFromSource()
    {
        EXPECT_CALL(*mock_svt_, Receive()).WillRepeatedly([this]() {
            return frame_source_.Get();
        });
    }

    std::shared_ptr<SvtMock> mock_svt_;
    FrameSource frame_source_;
    std::unique_ptr<detail::SvtCallbackDispatcher> dispatcher_;
};

// ---------------------------------------------------------------------------
// Worker lifecycle
// ---------------------------------------------------------------------------

TEST_F(SvtCallbackDispatcherTest, WorkerDoesNotPollWhileNoCallbackIsRegistered)
{
    dispatcher_->Start();
    EXPECT_CALL(*mock_svt_, Receive()).Times(0);

    std::this_thread::sleep_for(20 * kPollInterval);
}

TEST_F(SvtCallbackDispatcherTest, WorkerDoesNotPollBeforeStartEvenWithCallbackRegistered)
{
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    dispatcher_->SetStatusChangedCallback([](const VehicleTimeStatus&) {});

    std::this_thread::sleep_for(20 * kPollInterval);
    EXPECT_EQ(frame_source_.Polls(), 0U);
}

TEST_F(SvtCallbackDispatcherTest, WorkerStopsPollingAfterLastCallbackUnset)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    dispatcher_->UnsetStatusChangedCallback();

    const auto polls_after_unset = frame_source_.Polls();
    std::this_thread::sleep_for(20 * kPollInterval);
    EXPECT_EQ(frame_source_.Polls(), polls_after_unset);
}

TEST_F(SvtCallbackDispatcherTest, WorkerRestartsWhenCallbackReRegisteredAfterLastUnset)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    dispatcher_->UnsetStatusChangedCallback();
    const auto polls_after_unset = frame_source_.Polls();
    std::this_thread::sleep_for(20 * kPollInterval);
    ASSERT_EQ(frame_source_.Polls(), polls_after_unset);

    // Re-registering must restart the worker and resume polling.
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(1U));
    ASSERT_TRUE(recorder.WaitForCount(2U));
}

TEST_F(SvtCallbackDispatcherTest, NoCallbackIsDeliveredWhileReceiveReturnsNullopt)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(std::nullopt);

    Recorder<VehicleTimeStatus> status_recorder;
    Recorder<TimeSlaveSyncData<VehicleTime>> sync_recorder;
    Recorder<PDelayMeasurementData<VehicleTime>> pdelay_recorder;
    dispatcher_->SetStatusChangedCallback([&status_recorder](const VehicleTimeStatus& status) {
        status_recorder.Record(status);
    });
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&sync_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        sync_recorder.Record(data);
    });
    dispatcher_->SetPDelayMeasurementFinishedCallback([&pdelay_recorder](const PDelayMeasurementData<VehicleTime>& data) {
        pdelay_recorder.Record(data);
    });

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(status_recorder.Count(), 0U);
    EXPECT_EQ(sync_recorder.Count(), 0U);
    EXPECT_EQ(pdelay_recorder.Count(), 0U);
}

TEST_F(SvtCallbackDispatcherTest, DestructorJoinsWorkerWhileCallbacksAreRegistered)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    dispatcher_.reset();

    const auto polls_after_destruction = frame_source_.Polls();
    std::this_thread::sleep_for(20 * kPollInterval);
    EXPECT_EQ(frame_source_.Polls(), polls_after_destruction);
}

// ---------------------------------------------------------------------------
// VehicleTimeStatus
// ---------------------------------------------------------------------------

TEST_F(SvtCallbackDispatcherTest, StatusCallbackFiresOnFirstFrameAfterRegistration)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus, 2.5));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
    EXPECT_FALSE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));
    EXPECT_DOUBLE_EQ(recorder.Last().RateDeviation(), 2.5);
}

TEST_F(SvtCallbackDispatcherTest, StatusCallbackFiresForInvalidFrameWithEmptyFlags)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(SvtStatus{true, false, false, false, false}));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_FALSE(recorder.Last().IsConsistent());
}

TEST_F(SvtCallbackDispatcherTest, StatusCallbackDoesNotRepeatWhileFlagsAreUnchanged)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

TEST_F(SvtCallbackDispatcherTest, StatusCallbackFiresWhenFlagsChange)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    frame_source_.Set(MakeFrame(kTimeoutStatus));

    ASSERT_TRUE(recorder.WaitForCount(2U));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
}

TEST_F(SvtCallbackDispatcherTest, StatusCallbackIgnoresRateDeviationChanges)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus, 1.0));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    frame_source_.Set(MakeFrame(kSynchronizedStatus, 9.0));

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
    EXPECT_DOUBLE_EQ(recorder.Last().RateDeviation(), 1.0);
}

TEST_F(SvtCallbackDispatcherTest, StatusCallbackReRegisteredReceivesUnchangedStatusAgain)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> first_recorder;
    dispatcher_->SetStatusChangedCallback([&first_recorder](const VehicleTimeStatus& status) {
        first_recorder.Record(status);
    });
    ASSERT_TRUE(first_recorder.WaitForCount(1U));

    dispatcher_->UnsetStatusChangedCallback();

    Recorder<VehicleTimeStatus> second_recorder;
    dispatcher_->SetStatusChangedCallback([&second_recorder](const VehicleTimeStatus& status) {
        second_recorder.Record(status);
    });

    ASSERT_TRUE(second_recorder.WaitForCount(1U));
    EXPECT_TRUE(second_recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
    EXPECT_EQ(first_recorder.Count(), 1U);

    // Afterwards only changes are delivered.
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(second_recorder.Count(), 1U);
}

TEST_F(SvtCallbackDispatcherTest, StatusCallbackReplacedWithoutUnsetReceivesUnchangedStatusAgain)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> first_recorder;
    dispatcher_->SetStatusChangedCallback([&first_recorder](const VehicleTimeStatus& status) {
        first_recorder.Record(status);
    });
    ASSERT_TRUE(first_recorder.WaitForCount(1U));

    Recorder<VehicleTimeStatus> second_recorder;
    dispatcher_->SetStatusChangedCallback([&second_recorder](const VehicleTimeStatus& status) {
        second_recorder.Record(status);
    });

    ASSERT_TRUE(second_recorder.WaitForCount(1U));
    EXPECT_TRUE(second_recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kSynchronized));
    EXPECT_EQ(first_recorder.Count(), 1U);
}

TEST_F(SvtCallbackDispatcherTest, StatusCallbackRegisteredWhileWorkerIsActiveReceivesCurrentStatusFirst)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kTimeoutStatus));

    // Worker already running and has seen the timeout status through another callback.
    dispatcher_->SetPDelayMeasurementFinishedCallback([](const PDelayMeasurementData<VehicleTime>&) {});
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_TRUE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));

    frame_source_.Set(MakeFrame(kSynchronizedStatus));
    ASSERT_TRUE(recorder.WaitForCount(2U));
    EXPECT_FALSE(recorder.Last().IsFlagActive(VehicleTime::StatusFlag::kTimeOut));
}

TEST_F(SvtCallbackDispatcherTest, UnsetStatusCallbackStopsDelivery)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([&recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    dispatcher_->UnsetStatusChangedCallback();
    frame_source_.Set(MakeFrame(kTimeoutStatus));

    // Keep the worker polling through another registered callback and verify status stays silent.
    dispatcher_->SetPDelayMeasurementFinishedCallback([](const PDelayMeasurementData<VehicleTime>&) {});
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

// ---------------------------------------------------------------------------
// TimeSlaveSyncData
// ---------------------------------------------------------------------------

TEST_F(SvtCallbackDispatcherTest, SyncDataCallbackReceivesCurrentFrameOnRegistration)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data.sequence_id = 1U;
    frame_source_.Set(frame);

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        recorder.Record(data);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_EQ(recorder.Last().sequence_id, 1U);

    // Unchanged frame: no further delivery.
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

TEST_F(SvtCallbackDispatcherTest, SyncDataCallbackFiresOnEachNewFrame)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data.sequence_id = 1U;
    frame_source_.Set(frame);

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
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

TEST_F(SvtCallbackDispatcherTest, SyncDataCallbackRegisteredWhileWorkerIsActiveReceivesCurrentFrameFirst)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data.sequence_id = 1U;
    frame_source_.Set(frame);

    // Worker already running and has seen sequence 1 through another callback.
    dispatcher_->SetStatusChangedCallback([](const VehicleTimeStatus&) {});
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        recorder.Record(data);
    });

    ASSERT_TRUE(recorder.WaitForCount(1U));
    EXPECT_EQ(recorder.Last().sequence_id, 1U);

    frame.sync_fup_data.sequence_id = 2U;
    frame_source_.Set(frame);
    ASSERT_TRUE(recorder.WaitForCount(2U));
    EXPECT_EQ(recorder.Last().sequence_id, 2U);
}

TEST_F(SvtCallbackDispatcherTest, SyncDataCallbackReplacedWithoutUnsetReceivesCurrentFrameFirst)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.sync_fup_data.sequence_id = 1U;
    frame_source_.Set(frame);

    Recorder<TimeSlaveSyncData<VehicleTime>> first_recorder;
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&first_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        first_recorder.Record(data);
    });
    ASSERT_TRUE(first_recorder.WaitForCount(1U));

    Recorder<TimeSlaveSyncData<VehicleTime>> second_recorder;
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&second_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        second_recorder.Record(data);
    });

    ASSERT_TRUE(second_recorder.WaitForCount(1U));
    EXPECT_EQ(second_recorder.Last().sequence_id, 1U);
    EXPECT_EQ(first_recorder.Count(), 1U);
}

TEST_F(SvtCallbackDispatcherTest, SyncDataCallbackReceivesConvertedFields)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
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

TEST_F(SvtCallbackDispatcherTest, UnsetSyncDataCallbackStopsDelivery)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame_source_.Set(frame);

    Recorder<TimeSlaveSyncData<VehicleTime>> recorder;
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        recorder.Record(data);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));
    dispatcher_->UnsetTimeSlaveSyncDataReceivedCallback();

    // Keep the worker polling through another registered callback.
    dispatcher_->SetStatusChangedCallback([](const VehicleTimeStatus&) {});
    frame.sync_fup_data.sequence_id = 9U;
    frame_source_.Set(frame);

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

// ---------------------------------------------------------------------------
// PDelayMeasurementData
// ---------------------------------------------------------------------------

TEST_F(SvtCallbackDispatcherTest, PDelayCallbackFiresOnEachNewFrame)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame.pdelay_data.sequence_id = 1U;
    frame_source_.Set(frame);

    Recorder<PDelayMeasurementData<VehicleTime>> recorder;
    dispatcher_->SetPDelayMeasurementFinishedCallback([&recorder](const PDelayMeasurementData<VehicleTime>& data) {
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

TEST_F(SvtCallbackDispatcherTest, PDelayCallbackReceivesConvertedFields)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<PDelayMeasurementData<VehicleTime>> recorder;
    dispatcher_->SetPDelayMeasurementFinishedCallback([&recorder](const PDelayMeasurementData<VehicleTime>& data) {
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

TEST_F(SvtCallbackDispatcherTest, UnsetPDelayCallbackStopsDelivery)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame_source_.Set(frame);

    Recorder<PDelayMeasurementData<VehicleTime>> recorder;
    dispatcher_->SetPDelayMeasurementFinishedCallback([&recorder](const PDelayMeasurementData<VehicleTime>& data) {
        recorder.Record(data);
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));
    dispatcher_->UnsetPDelayMeasurementFinishedCallback();

    // Keep the worker polling through another registered callback.
    dispatcher_->SetStatusChangedCallback([](const VehicleTimeStatus&) {});
    frame.pdelay_data.sequence_id = 9U;
    frame_source_.Set(frame);

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

// ---------------------------------------------------------------------------
// Multiple callbacks and thread safety of Set / Unset
// ---------------------------------------------------------------------------

TEST_F(SvtCallbackDispatcherTest, AllThreeCallbacksAreDeliveredFromTheSameFrame)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame_source_.Set(frame);

    Recorder<VehicleTimeStatus> status_recorder;
    Recorder<TimeSlaveSyncData<VehicleTime>> sync_recorder;
    Recorder<PDelayMeasurementData<VehicleTime>> pdelay_recorder;
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&sync_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
        sync_recorder.Record(data);
    });
    dispatcher_->SetPDelayMeasurementFinishedCallback([&pdelay_recorder](const PDelayMeasurementData<VehicleTime>& data) {
        pdelay_recorder.Record(data);
    });
    dispatcher_->SetStatusChangedCallback([&status_recorder](const VehicleTimeStatus& status) {
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

TEST_F(SvtCallbackDispatcherTest, UnsetFromWithinCallbackDoesNotDeadlock)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> recorder;
    dispatcher_->SetStatusChangedCallback([this, &recorder](const VehicleTimeStatus& status) {
        recorder.Record(status);
        dispatcher_->UnsetStatusChangedCallback();
    });
    ASSERT_TRUE(recorder.WaitForCount(1U));

    // Keep the worker polling through another registered callback; the status callback must stay silent.
    dispatcher_->SetPDelayMeasurementFinishedCallback([](const PDelayMeasurementData<VehicleTime>&) {});
    frame_source_.Set(MakeFrame(kTimeoutStatus));

    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(recorder.Count(), 1U);
}

TEST_F(SvtCallbackDispatcherTest, SubscribingToAnotherEventFromWithinCallbackDoesNotDeadlock)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    auto frame = MakeFrame(kSynchronizedStatus);
    frame_source_.Set(frame);

    Recorder<VehicleTimeStatus> status_recorder;
    Recorder<TimeSlaveSyncData<VehicleTime>> sync_recorder;
    dispatcher_->SetStatusChangedCallback([this, &status_recorder, &sync_recorder](const VehicleTimeStatus& status) {
        status_recorder.Record(status);
        dispatcher_->SetTimeSlaveSyncDataReceivedCallback([&sync_recorder](const TimeSlaveSyncData<VehicleTime>& data) {
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

TEST_F(SvtCallbackDispatcherTest, UnsetBlocksUntilInFlightCallbackReturns)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    std::promise<void> callback_entered;
    std::promise<void> release_callback;
    auto release_future = release_callback.get_future().share();
    dispatcher_->SetStatusChangedCallback([&callback_entered, release_future](const VehicleTimeStatus&) {
        callback_entered.set_value();
        release_future.wait();
    });
    ASSERT_EQ(callback_entered.get_future().wait_for(kWaitTimeout), std::future_status::ready);

    auto unset_done = std::async(std::launch::async, [this]() {
        dispatcher_->UnsetStatusChangedCallback();
    });
    EXPECT_EQ(unset_done.wait_for(50 * kPollInterval), std::future_status::timeout);

    release_callback.set_value();
    EXPECT_EQ(unset_done.wait_for(kWaitTimeout), std::future_status::ready);
}

TEST_F(SvtCallbackDispatcherTest, LastCallbackUnsettingItselfAndRegisteringAnotherRestartsWorker)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    Recorder<VehicleTimeStatus> status_recorder;
    Recorder<PDelayMeasurementData<VehicleTime>> pdelay_recorder;

    // The sole callback drops the worker to zero subscriptions from the worker thread and, in the
    // same invocation, registers a different one — exercising the deferred-stop / self-detach path.
    dispatcher_->SetStatusChangedCallback(
        [this, &status_recorder, &pdelay_recorder](const VehicleTimeStatus& status) {
            status_recorder.Record(status);
            dispatcher_->UnsetStatusChangedCallback();
            dispatcher_->SetPDelayMeasurementFinishedCallback(
                [&pdelay_recorder](const PDelayMeasurementData<VehicleTime>& data) {
                    pdelay_recorder.Record(data);
                });
        });

    ASSERT_TRUE(status_recorder.WaitForCount(1U));

    // The newly registered pDelay callback must be serviced by the restarted worker.
    ASSERT_TRUE(pdelay_recorder.WaitForCount(1U));

    // The status callback removed itself, so it must not fire again.
    ASSERT_TRUE(frame_source_.WaitForAdditionalPolls(5U));
    EXPECT_EQ(status_recorder.Count(), 1U);
}

TEST_F(SvtCallbackDispatcherTest, SettingEmptyCallbackDoesNotInvokeAnything)
{
    dispatcher_->Start();
    ServeFramesFromSource();
    frame_source_.Set(MakeFrame(kSynchronizedStatus));

    dispatcher_->SetStatusChangedCallback(VehicleTime::StatusChangedCallback{});
    dispatcher_->SetTimeSlaveSyncDataReceivedCallback(VehicleTime::TimeSlaveSyncDataReceivedCallback{});
    dispatcher_->SetPDelayMeasurementFinishedCallback(VehicleTime::PDelayMeasurementFinishedCallback{});

    // No callback is registered, so the worker must not poll at all.
    std::this_thread::sleep_for(20 * kPollInterval);
    EXPECT_EQ(frame_source_.Polls(), 0U);
}

}  // namespace
}  // namespace time
}  // namespace score
