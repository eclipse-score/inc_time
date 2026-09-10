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

#include <score/utility.hpp>

#include <string>
#include <utility>

namespace score
{
namespace time
{
namespace detail
{

namespace
{

/// @brief Reinterprets an unsigned nanosecond count from the IPC layer as a signed chrono duration.
std::chrono::nanoseconds ToNanoseconds(const std::uint64_t nanoseconds) noexcept
{
    return std::chrono::nanoseconds{static_cast<std::chrono::nanoseconds::rep>(nanoseconds)};
}

PortIdentity ToPortIdentity(const std::uint64_t clock_identity, const std::uint32_t port_number) noexcept
{
    PortIdentity identity{};
    identity.clock_identity = clock_identity;
    identity.port_number = static_cast<std::uint16_t>(port_number);
    return identity;
}

}  // namespace

ClockStatus<VehicleTime::StatusFlag> ConvertPtpStatus(const score::td::svt::TimeBaseStatus& ptp_status) noexcept
{
    using Flag = VehicleTime::StatusFlag;
    if (!ptp_status.is_correct)
    {
        return ClockStatus<Flag>{};
    }
    ClockStatus<Flag> status;
    if (ptp_status.is_synchronized)
    {
        status.AddFlag(Flag::kSynchronized);
    }
    if (ptp_status.is_timeout)
    {
        status.AddFlag(Flag::kTimeOut);
    }
    if (ptp_status.is_time_jump_future)
    {
        status.AddFlag(Flag::kTimeLeapFuture);
    }
    if (ptp_status.is_time_jump_past)
    {
        status.AddFlag(Flag::kTimeLeapPast);
    }
    return status;
}

SvtCallbackDispatcher::SvtCallbackDispatcher(std::shared_ptr<score::td::SvtReceiver> receiver,
                                             const std::chrono::milliseconds poll_interval) noexcept
    : svt_receiver_{std::move(receiver)},
      poll_interval_{poll_interval},
      sync_data_slot_{},
      pdelay_slot_{},
      status_slot_{},
      worker_mutex_{},
      worker_wakeup_{},
      worker_{}
{
}

SvtCallbackDispatcher::~SvtCallbackDispatcher() noexcept
{
    if (worker_.joinable())
    {
        {
            const std::lock_guard<std::mutex> guard{worker_mutex_};
            score::cpp::ignore = worker_.request_stop();
            worker_wakeup_.notify_all();
        }
        worker_.join();
    }
}

void SvtCallbackDispatcher::Start() noexcept
{
    worker_ = score::cpp::jthread{score::cpp::jthread::name_hint{std::string{"vt_cb_dispatch"}},
                                  [this](const score::cpp::stop_token token) noexcept {
                                      WorkerFunction(token);
                                  }};
}

void SvtCallbackDispatcher::SetTimeSlaveSyncDataReceivedCallback(
    VehicleTime::TimeSlaveSyncDataReceivedCallback&& callback) noexcept
{
    sync_data_slot_.Set(std::move(callback));
}

void SvtCallbackDispatcher::UnsetTimeSlaveSyncDataReceivedCallback() noexcept
{
    sync_data_slot_.Unset();
}

void SvtCallbackDispatcher::SetPDelayMeasurementFinishedCallback(
    VehicleTime::PDelayMeasurementFinishedCallback&& callback) noexcept
{
    pdelay_slot_.Set(std::move(callback));
}

void SvtCallbackDispatcher::UnsetPDelayMeasurementFinishedCallback() noexcept
{
    pdelay_slot_.Unset();
}

void SvtCallbackDispatcher::SetStatusChangedCallback(VehicleTime::StatusChangedCallback&& callback) noexcept
{
    status_slot_.Set(std::move(callback));
}

void SvtCallbackDispatcher::UnsetStatusChangedCallback() noexcept
{
    status_slot_.Unset();
}

void SvtCallbackDispatcher::WorkerFunction(const score::cpp::stop_token& token) noexcept
{
    while (!token.stop_requested())
    {
        if (IsAnyCallbackSet())
        {
            PollAndDispatch();
        }

        std::unique_lock<std::mutex> lock{worker_mutex_};
        score::cpp::ignore = worker_wakeup_.wait_for(lock, token, poll_interval_, [&token]() noexcept -> bool {
            return token.stop_requested();
        });
    }
}

bool SvtCallbackDispatcher::IsAnyCallbackSet() const noexcept
{
    return sync_data_slot_.IsSet() || pdelay_slot_.IsSet() || status_slot_.IsSet();
}

void SvtCallbackDispatcher::PollAndDispatch() noexcept
{
    const auto frame = svt_receiver_->Receive();
    if (!frame.has_value())
    {
        return;
    }

    const auto& sync_data = frame.value().sync_fup_data;
    score::cpp::ignore = sync_data_slot_.InvokeIfChanged(sync_data, ConvertSyncData(sync_data));

    const auto& pdelay_data = frame.value().pdelay_data;
    score::cpp::ignore = pdelay_slot_.InvokeIfChanged(pdelay_data, ConvertPDelayData(pdelay_data));

    const auto status_flags = ConvertPtpStatus(frame.value().status);
    score::cpp::ignore =
        status_slot_.InvokeIfChanged(status_flags, VehicleTimeStatus{status_flags, frame.value().rate_deviation});
}

TimeSlaveSyncData<VehicleTime> SvtCallbackDispatcher::ConvertSyncData(
    const score::td::svt::SyncFupSnapshot& sync_data) noexcept
{
    TimeSlaveSyncData<VehicleTime> converted{};
    converted.precise_origin_timestamp = VehicleTime::Timepoint{ToNanoseconds(sync_data.precise_origin_timestamp)};
    converted.reference_global_timestamp = VehicleTime::Timepoint{ToNanoseconds(sync_data.reference_global_timestamp)};
    converted.reference_local_timestamp = LocalPTPDeviceTimerValue{ToNanoseconds(sync_data.reference_local_timestamp)};
    converted.sync_ingress_timestamp = LocalPTPDeviceTimerValue{ToNanoseconds(sync_data.sync_ingress_timestamp)};
    converted.correction_field = static_cast<std::int64_t>(sync_data.correction_field);
    converted.sequence_id = sync_data.sequence_id;
    converted.pdelay = ToNanoseconds(sync_data.pdelay);
    converted.source_port_identity = ToPortIdentity(sync_data.clock_identity, sync_data.port_number);
    return converted;
}

PDelayMeasurementData<VehicleTime> SvtCallbackDispatcher::ConvertPDelayData(
    const score::td::svt::PDelayDataSnapshot& pdelay_data) noexcept
{
    PDelayMeasurementData<VehicleTime> converted{};
    converted.request_origin_timestamp = LocalPTPDeviceTimerValue{ToNanoseconds(pdelay_data.request_origin_timestamp)};
    converted.request_receipt_timestamp =
        MasterPTPDeviceTimerValue{ToNanoseconds(pdelay_data.request_receipt_timestamp)};
    converted.response_origin_timestamp =
        MasterPTPDeviceTimerValue{ToNanoseconds(pdelay_data.response_origin_timestamp)};
    converted.response_receipt_timestamp =
        LocalPTPDeviceTimerValue{ToNanoseconds(pdelay_data.response_receipt_timestamp)};
    converted.reference_global_timestamp =
        VehicleTime::Timepoint{ToNanoseconds(pdelay_data.reference_global_timestamp)};
    converted.reference_local_timestamp =
        LocalPTPDeviceTimerValue{ToNanoseconds(pdelay_data.reference_local_timestamp)};
    converted.sequence_id = pdelay_data.sequence_id;
    converted.pdelay = ToNanoseconds(pdelay_data.pdelay);
    converted.request_port_identity = ToPortIdentity(pdelay_data.req_clock_identity, pdelay_data.req_port_number);
    converted.response_port_identity = ToPortIdentity(pdelay_data.resp_clock_identity, pdelay_data.resp_port_number);
    return converted;
}

}  // namespace detail
}  // namespace time
}  // namespace score
