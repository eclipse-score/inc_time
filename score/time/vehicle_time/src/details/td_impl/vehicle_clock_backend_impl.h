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
#ifndef SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_VEHICLE_CLOCK_BACKEND_IMPL_H
#define SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_VEHICLE_CLOCK_BACKEND_IMPL_H

// Internal header — include ONLY from vehicle_clock_backend_impl.cpp and vehicle_clock_backend_impl_test.cpp.
// NOT part of the public API of td_impl.

#include "score/concurrency/condition_variable.h"
#include "score/time/high_res_steady_time/src/high_res_steady_clock.h"
#include "score/time/vehicle_time/src/details/td_impl/callback_slot.h"
#include "score/time/vehicle_time/src/vehicle_clock.h"
#include "score/time/vehicle_time/src/vehicle_clock_backend.h"
#include "score/time_daemon/src/ipc/svt/receiver/svt_receiver.h"
#include "score/time_daemon/src/ipc/svt/svt_time_info.h"

#include <score/jthread.hpp>
#include <score/stop_token.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

namespace score
{
namespace time
{
namespace detail
{

/// @brief Production backend for the vehicle time domain.
///
/// Implements @c VehicleClockBackend by reading live PTP data from the TimeDaemon
/// via @c score::td::SvtReceiver.  The adjusted vehicle time is computed as:
///
///   adjusted_ptp = ptp_stamp_at_capture + (local_now - local_at_capture)
///
/// where the local reference clock is supplied via @c HighResSteadyClock::GetInstance()
/// (captured once at construction to avoid per-call mutex overhead).
///
/// @par Callback delivery
/// The TimeDaemon IPC is a shared-memory segment without a notification facility, so the
/// backend owns a dedicated worker thread that polls the receiver every @p poll_interval
/// while at least one callback is registered (no polling happens without subscribers).
/// The worker is started by the first successful @c Init() and joined in the destructor.
/// Every frame read from the receiver is offered part by part to the three @c CallbackSlot s, which
/// dispatch on the worker thread when the part differs from what they last delivered:
///  - @c TimeSlaveSyncData — the sync/follow-up part;
///  - @c PDelayMeasurementData — the pDelay part;
///  - @c VehicleTimeStatus — keyed on the status flags (rate deviation excluded from the comparison).
/// A newly registered callback (first registration, re-registration, or replacement) always receives
/// the first frame polled after its registration, and afterwards only changes.
///
/// Set/Unset are safe to call concurrently with an in-flight invocation (see @c CallbackSlot).
///
/// @note Placed in @c score::time::detail (rather than an anonymous namespace) so
/// that vehicle_clock_backend_impl_test.cpp can construct it directly with injected mocks.
/// Only one backend translation unit must be linked per binary to avoid ODR issues.
class VehicleClockBackendImpl final : public VehicleClockBackend
{
  public:
    /// @brief Default interval at which the worker thread polls the receiver for new frames.
    static constexpr std::chrono::milliseconds kDefaultPollInterval{50};

    VehicleClockBackendImpl(std::shared_ptr<score::td::SvtReceiver> receiver,
                            HighResSteadyClock local_clock,
                            std::chrono::milliseconds poll_interval = kDefaultPollInterval) noexcept;

    ~VehicleClockBackendImpl() noexcept override;
    VehicleClockBackendImpl(const VehicleClockBackendImpl&) = delete;
    VehicleClockBackendImpl& operator=(const VehicleClockBackendImpl&) = delete;
    VehicleClockBackendImpl(VehicleClockBackendImpl&&) = delete;
    VehicleClockBackendImpl& operator=(VehicleClockBackendImpl&&) = delete;

    ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus> Now() const noexcept override;

    bool Init() noexcept override;

    bool IsAvailable() const noexcept override;

    bool WaitUntilAvailable(const score::cpp::stop_token& token,
                            std::chrono::steady_clock::time_point until) const noexcept override;

    void SetTimeSlaveSyncDataReceivedCallback(
        VehicleTime::TimeSlaveSyncDataReceivedCallback&& callback) noexcept override;

    void UnsetTimeSlaveSyncDataReceivedCallback() noexcept override;

    void SetPDelayMeasurementFinishedCallback(
        VehicleTime::PDelayMeasurementFinishedCallback&& callback) noexcept override;

    void UnsetPDelayMeasurementFinishedCallback() noexcept override;

    void SetStatusChangedCallback(VehicleTime::StatusChangedCallback&& callback) noexcept override;

    void UnsetStatusChangedCallback() noexcept override;

  private:
    /// @brief Converts PTP status flags from the TimeDaemon IPC representation to
    ///        the @c ClockStatus<VehicleTime::StatusFlag> representation.
    static ClockStatus<VehicleTime::StatusFlag> ConvertPtpStatus(
        const score::td::svt::TimeBaseStatus& ptp_status) noexcept;

    /// @brief Converts the IPC sync/follow-up snapshot to the public event type.
    static TimeSlaveSyncData<VehicleTime> ConvertSyncData(const score::td::svt::SyncFupSnapshot& sync_data) noexcept;

    /// @brief Converts the IPC pDelay snapshot to the public event type.
    static PDelayMeasurementData<VehicleTime> ConvertPDelayData(
        const score::td::svt::PDelayDataSnapshot& pdelay_data) noexcept;

    /// @brief Starts the worker thread. Must be called at most once (guarded by @c init_mutex_).
    void StartWorker() noexcept;

    /// @brief Requests the worker thread to stop and joins it. Safe to call when it was never started.
    void StopWorker() noexcept;

    /// @brief Worker thread body: polls the receiver at @c poll_interval_ while callbacks are registered.
    void WorkerFunction(const score::cpp::stop_token& token) noexcept;

    /// @brief Returns @c true if at least one callback is currently registered.
    bool IsAnyCallbackSet() const noexcept;

    /// @brief Reads one frame from the receiver and offers each part to its slot.
    void PollAndDispatch() noexcept;

    std::atomic_bool is_ready_;
    std::mutex init_mutex_;
    std::shared_ptr<score::td::SvtReceiver> svt_receiver_;
    HighResSteadyClock local_clock_;

    const std::chrono::milliseconds poll_interval_;

    CallbackSlot<VehicleTime::TimeSlaveSyncDataReceivedCallback, score::td::svt::SyncFupSnapshot> sync_data_slot_;
    CallbackSlot<VehicleTime::PDelayMeasurementFinishedCallback, score::td::svt::PDelayDataSnapshot> pdelay_slot_;
    CallbackSlot<VehicleTime::StatusChangedCallback, ClockStatus<VehicleTime::StatusFlag>> status_slot_;

    std::mutex worker_mutex_;
    score::concurrency::InterruptibleConditionalVariable worker_wakeup_;
    score::cpp::jthread worker_;
};

}  // namespace detail
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_VEHICLE_CLOCK_BACKEND_IMPL_H
