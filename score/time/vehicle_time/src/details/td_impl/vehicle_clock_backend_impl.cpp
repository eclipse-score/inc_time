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

#include <chrono>
#include <thread>
#include <utility>

namespace score
{
namespace time
{
namespace detail
{

VehicleClockBackendImpl::VehicleClockBackendImpl(std::shared_ptr<score::td::SvtReceiver> receiver,
                                                 HighResSteadyClock local_clock,
                                                 const std::chrono::milliseconds poll_interval) noexcept
    : is_ready_{false},
      init_mutex_{},
      svt_receiver_{std::move(receiver)},
      local_clock_{std::move(local_clock)},
      dispatcher_{svt_receiver_, poll_interval}
{
}

VehicleClockBackendImpl::~VehicleClockBackendImpl() noexcept = default;

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
    dispatcher_.Start();
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
    dispatcher_.SetTimeSlaveSyncDataReceivedCallback(std::move(callback));
}

void VehicleClockBackendImpl::UnsetTimeSlaveSyncDataReceivedCallback() noexcept
{
    dispatcher_.UnsetTimeSlaveSyncDataReceivedCallback();
}

void VehicleClockBackendImpl::SetPDelayMeasurementFinishedCallback(
    VehicleTime::PDelayMeasurementFinishedCallback&& callback) noexcept
{
    dispatcher_.SetPDelayMeasurementFinishedCallback(std::move(callback));
}

void VehicleClockBackendImpl::UnsetPDelayMeasurementFinishedCallback() noexcept
{
    dispatcher_.UnsetPDelayMeasurementFinishedCallback();
}

void VehicleClockBackendImpl::SetStatusChangedCallback(VehicleTime::StatusChangedCallback&& callback) noexcept
{
    dispatcher_.SetStatusChangedCallback(std::move(callback));
}

void VehicleClockBackendImpl::UnsetStatusChangedCallback() noexcept
{
    dispatcher_.UnsetStatusChangedCallback();
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
