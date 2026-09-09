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
#include "score/time/vehicle_time/src/details/logging_contexts.h"

#include "score/time_daemon/src/ipc/svt/receiver/factory.h"
#include "score/time_daemon/src/ipc/svt/svt_time_info.h"

#include "score/mw/log/logging.h"

#include <score/utility.hpp>

#include <chrono>
#include <string>
#include <thread>
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

VehicleClockBackendImpl::VehicleClockBackendImpl(std::shared_ptr<score::td::SvtReceiver> receiver,
                                                 HighResSteadyClock local_clock,
                                                 const std::chrono::milliseconds poll_interval) noexcept
    : is_ready_{false},
      init_mutex_{},
      svt_receiver_{std::move(receiver)},
      local_clock_{std::move(local_clock)},
      poll_interval_{poll_interval},
      sync_data_slot_{},
      pdelay_slot_{},
      status_slot_{},
      baselined_sync_generation_{0U},
      last_sync_data_{},
      baselined_pdelay_generation_{0U},
      last_pdelay_data_{},
      delivered_status_generation_{0U},
      last_delivered_status_flags_{},
      worker_mutex_{},
      worker_wakeup_{},
      worker_{}
{
}

VehicleClockBackendImpl::~VehicleClockBackendImpl() noexcept
{
    StopWorker();
}

ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus> VehicleClockBackendImpl::Now() const noexcept
{
    const ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus> kEmptySnapshot{VehicleTime::Timepoint{},
                                                                                  VehicleTimeStatus{}};

    if (!is_ready_)
    {
        return kEmptySnapshot;
    }

    const auto rx_data = svt_receiver_->Receive();
    if (!rx_data.has_value())
    {
        return kEmptySnapshot;
    }

    const auto now_local = local_clock_.Now().TimePoint().time_since_epoch();
    const auto local_at_capture = std::chrono::nanoseconds{rx_data.value().local_time};
    const auto ptp_at_capture = std::chrono::nanoseconds{rx_data.value().ptp_assumed_time};

    if (now_local < local_at_capture)
    {
        score::mw::log::LogError(kVehicleTimeLogContext)
            << "Local clock is behind PTP capture reference — returning empty status.";
        return kEmptySnapshot;
    }

    const VehicleTime::Timepoint adjusted_tp{ptp_at_capture + (now_local - local_at_capture)};

    const VehicleTimeStatus vehicle_status{ConvertPtpStatus(rx_data.value().status), rx_data.value().rate_deviation};
    return ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus>{adjusted_tp, vehicle_status};
}

bool VehicleClockBackendImpl::Init() noexcept
{
    if (is_ready_.load(std::memory_order_acquire))
    {
        return true;
    }

    const std::lock_guard<std::mutex> init_guard{init_mutex_};

    // Lets check if another thread completed init while we waited for the lock
    if (is_ready_.load(std::memory_order_relaxed))
    {
        return true;
    }

    const bool ok = svt_receiver_->Init();
    if (!ok)
    {
        score::mw::log::LogError(kVehicleTimeLogContext)
            << "VehicleClockBackendImpl: failed to open TimeDaemon shared memory segment.";
        return false;
    }

    // The worker only ever reads from the receiver, so it must not run before the receiver is initialised.
    StartWorker();
    is_ready_.store(true, std::memory_order_release);

    return true;
}

bool VehicleClockBackendImpl::IsAvailable() const noexcept
{
    return is_ready_.load(std::memory_order_acquire);
}

bool VehicleClockBackendImpl::WaitUntilAvailable(const score::cpp::stop_token& token,
                                                 std::chrono::steady_clock::time_point until) const noexcept
{
    bool should_poll = false;
    do
    {
        if (IsAvailable())
        {
            return true;
        }
        // Poll at 10 ms intervals: coarse enough to avoid busy-spinning, fine enough
        // to detect IPC readiness well within any realistic startup deadline.
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        should_poll = (!token.stop_requested()) && (std::chrono::steady_clock::now() <= until);
    } while (should_poll);

    score::mw::log::LogError(kVehicleTimeLogContext) << "Vehicle time IPC to TimeDaemon not ready within deadline.";
    return false;
}

void VehicleClockBackendImpl::SetTimeSlaveSyncDataReceivedCallback(
    VehicleTime::TimeSlaveSyncDataReceivedCallback&& callback) noexcept
{
    sync_data_slot_.Set(std::move(callback));
}

void VehicleClockBackendImpl::UnsetTimeSlaveSyncDataReceivedCallback() noexcept
{
    sync_data_slot_.Unset();
}

void VehicleClockBackendImpl::SetPDelayMeasurementFinishedCallback(
    VehicleTime::PDelayMeasurementFinishedCallback&& callback) noexcept
{
    pdelay_slot_.Set(std::move(callback));
}

void VehicleClockBackendImpl::UnsetPDelayMeasurementFinishedCallback() noexcept
{
    pdelay_slot_.Unset();
}

void VehicleClockBackendImpl::SetStatusChangedCallback(VehicleTime::StatusChangedCallback&& callback) noexcept
{
    status_slot_.Set(std::move(callback));
}

void VehicleClockBackendImpl::UnsetStatusChangedCallback() noexcept
{
    status_slot_.Unset();
}

void VehicleClockBackendImpl::StartWorker() noexcept
{
    worker_ = score::cpp::jthread{score::cpp::jthread::name_hint{std::string{"vt_cb_dispatch"}},
                                  [this](const score::cpp::stop_token token) noexcept {
                                      WorkerFunction(token);
                                  }};
}

void VehicleClockBackendImpl::StopWorker() noexcept
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

void VehicleClockBackendImpl::WorkerFunction(const score::cpp::stop_token& token) noexcept
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

bool VehicleClockBackendImpl::IsAnyCallbackSet() const noexcept
{
    return sync_data_slot_.IsSet() || pdelay_slot_.IsSet() || status_slot_.IsSet();
}

void VehicleClockBackendImpl::PollAndDispatch() noexcept
{
    const auto frame = svt_receiver_->Receive();
    if (!frame.has_value())
    {
        return;
    }

    DispatchTimeSlaveSyncData(frame.value());
    DispatchPDelayMeasurementData(frame.value());
    DispatchStatus(frame.value());
}

void VehicleClockBackendImpl::DispatchTimeSlaveSyncData(const score::td::svt::TimeBaseSnapshot& frame) noexcept
{
    const auto generation = sync_data_slot_.Generation();
    if (generation != baselined_sync_generation_)
    {
        // Fresh registration: the frame currently in shared memory is the baseline, not a new frame.
        baselined_sync_generation_ = generation;
        last_sync_data_ = frame.sync_fup_data;
        return;
    }

    if (last_sync_data_.has_value() && (last_sync_data_.value() == frame.sync_fup_data))
    {
        return;
    }

    last_sync_data_ = frame.sync_fup_data;
    score::cpp::ignore = sync_data_slot_.Invoke(ConvertSyncData(frame.sync_fup_data));
}

void VehicleClockBackendImpl::DispatchPDelayMeasurementData(const score::td::svt::TimeBaseSnapshot& frame) noexcept
{
    const auto generation = pdelay_slot_.Generation();
    if (generation != baselined_pdelay_generation_)
    {
        // Fresh registration: the frame currently in shared memory is the baseline, not a new frame.
        baselined_pdelay_generation_ = generation;
        last_pdelay_data_ = frame.pdelay_data;
        return;
    }

    if (last_pdelay_data_.has_value() && (last_pdelay_data_.value() == frame.pdelay_data))
    {
        return;
    }

    last_pdelay_data_ = frame.pdelay_data;
    score::cpp::ignore = pdelay_slot_.Invoke(ConvertPDelayData(frame.pdelay_data));
}

void VehicleClockBackendImpl::DispatchStatus(const score::td::svt::TimeBaseSnapshot& frame) noexcept
{
    const auto flags = ConvertPtpStatus(frame.status);

    const bool first_after_registration = (status_slot_.Generation() != delivered_status_generation_);
    const bool flags_changed =
        (!last_delivered_status_flags_.has_value()) || (!(last_delivered_status_flags_.value() == flags));
    if (!(first_after_registration || flags_changed))
    {
        return;
    }

    const VehicleTimeStatus status{flags, frame.rate_deviation};
    const auto invoked_generation = status_slot_.Invoke(status);
    if (invoked_generation.has_value())
    {
        delivered_status_generation_ = invoked_generation.value();
        last_delivered_status_flags_ = flags;
    }
}

ClockStatus<VehicleTime::StatusFlag> VehicleClockBackendImpl::ConvertPtpStatus(
    const score::td::svt::TimeBaseStatus& ptp_status) noexcept
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

TimeSlaveSyncData<VehicleTime> VehicleClockBackendImpl::ConvertSyncData(
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

PDelayMeasurementData<VehicleTime> VehicleClockBackendImpl::ConvertPDelayData(
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

template <>
std::shared_ptr<VehicleClockBackend> detail::CreateBackend<VehicleTime>()
{
    return std::make_shared<detail::VehicleClockBackendImpl>(score::td::CreateSvtReceiver(),
                                                             HighResSteadyClock::GetInstance());
}

}  // namespace time
}  // namespace score
