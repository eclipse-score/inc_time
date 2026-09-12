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
#ifndef SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_SVT_CALLBACK_DISPATCHER_H
#define SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_SVT_CALLBACK_DISPATCHER_H

// Internal header — include ONLY from translation units under vehicle_time/src/details/td_impl/.
// NOT part of the public API of td_impl.

#include "score/concurrency/condition_variable.h"
#include "score/time/vehicle_time/src/details/td_impl/svt_callback_wrapper.h"
#include "score/time/vehicle_time/src/vehicle_clock.h"
#include "score/time_daemon/src/ipc/svt/receiver/svt_receiver.h"
#include "score/time_daemon/src/ipc/svt/svt_time_info.h"

#include <score/jthread.hpp>
#include <score/stop_token.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

namespace score
{
namespace time
{
namespace detail
{

/// @brief Converts PTP status flags from the TimeDaemon IPC representation to
///        the @c ClockStatus<VehicleTime::StatusFlag> representation.
ClockStatus<VehicleTime::StatusFlag> ConvertPtpStatus(const score::td::svt::TimeBaseStatus& ptp_status) noexcept;

/// @brief Owns the worker thread that delivers vehicle-time subscription callbacks.
///
/// The TimeDaemon IPC is a shared-memory segment without a notification facility, so the
/// dispatcher owns a dedicated worker thread that polls the receiver every @p poll_interval.
/// @c Start() enables dispatching once the receiver is initialised; after that the worker thread
/// is created lazily when the first callback is registered and joined again when the last callback
/// is removed. Registering a callback afterwards restarts the worker.
/// Every frame read from the receiver is offered part by part to the three @c SvtCallbackWrapper s, which
/// dispatch on the worker thread when the part differs from what they last delivered:
///  - @c TimeSlaveSyncData — the sync/follow-up part;
///  - @c PDelayMeasurementData — the pDelay part;
///  - @c VehicleTimeStatus — keyed on the status flags (rate deviation excluded from the comparison).
/// A newly registered callback (first registration, re-registration, or replacement) always receives
/// the first frame polled after its registration, and afterwards only changes.
///
/// Set/Unset are safe to call concurrently with an in-flight invocation (see @c SvtCallbackWrapper),
/// including a callback that unsets or replaces itself from the worker thread.
class SvtCallbackDispatcher final
{
  public:
    SvtCallbackDispatcher(std::shared_ptr<score::td::SvtReceiver> receiver,
                          std::chrono::milliseconds poll_interval) noexcept;

    ~SvtCallbackDispatcher() noexcept;
    SvtCallbackDispatcher(const SvtCallbackDispatcher&) = delete;
    SvtCallbackDispatcher& operator=(const SvtCallbackDispatcher&) = delete;
    SvtCallbackDispatcher(SvtCallbackDispatcher&&) = delete;
    SvtCallbackDispatcher& operator=(SvtCallbackDispatcher&&) = delete;

    /// @brief Enables dispatching once the receiver is initialised. Must be called at most once.
    ///
    /// The worker thread itself is only spun up while at least one callback is registered.
    void Start() noexcept;

    void SetTimeSlaveSyncDataReceivedCallback(VehicleTime::TimeSlaveSyncDataReceivedCallback&& callback) noexcept;

    void UnsetTimeSlaveSyncDataReceivedCallback() noexcept;

    void SetPDelayMeasurementFinishedCallback(VehicleTime::PDelayMeasurementFinishedCallback&& callback) noexcept;

    void UnsetPDelayMeasurementFinishedCallback() noexcept;

    void SetStatusChangedCallback(VehicleTime::StatusChangedCallback&& callback) noexcept;

    void UnsetStatusChangedCallback() noexcept;

  private:
    /// @brief Converts the IPC sync/follow-up snapshot to the public event type.
    static TimeSlaveSyncData<VehicleTime> ConvertSyncData(const score::td::svt::SyncFupSnapshot& sync_data) noexcept;

    /// @brief Converts the IPC pDelay snapshot to the public event type.
    static PDelayMeasurementData<VehicleTime> ConvertPDelayData(
        const score::td::svt::PDelayDataSnapshot& pdelay_data) noexcept;

    /// @brief Worker thread body: polls the receiver at @c poll_interval_ while callbacks are registered.
    void WorkerFunction(const score::cpp::stop_token& token) noexcept;

    /// @brief Returns @c true if at least one callback is currently registered.
    bool IsAnyCallbackSet() const noexcept;

    /// @brief Reads one frame from the receiver and offers each part to its slot.
    void PollAndDispatch() noexcept;

    /// @brief Starts or stops the worker so it runs exactly while enabled and at least one callback
    ///        is registered. Must be called with @c lifecycle_mutex_ held.
    void ManageWorkerThreadLifecycleLocked() noexcept;

    /// @brief Returns @c true when called from the worker thread (used to defer a self-triggered join).
    bool OnWorkerThread() const noexcept;

    std::shared_ptr<score::td::SvtReceiver> svt_receiver_;
    const std::chrono::milliseconds poll_interval_;

    SvtCallbackWrapper<VehicleTime::TimeSlaveSyncDataReceivedCallback, score::td::svt::SyncFupSnapshot> sync_data_slot_;
    SvtCallbackWrapper<VehicleTime::PDelayMeasurementFinishedCallback, score::td::svt::PDelayDataSnapshot> pdelay_slot_;
    SvtCallbackWrapper<VehicleTime::StatusChangedCallback, ClockStatus<VehicleTime::StatusFlag>> status_slot_;

    // Guards the worker lifecycle state below. Never held while a slot mutex is taken, so that a
    // callback may (re-)register from the worker thread without risking a lock-order inversion.
    std::mutex lifecycle_mutex_;
    bool enabled_;
    bool sync_present_;
    bool pdelay_present_;
    bool status_present_;
    bool worker_active_;
    std::atomic<std::thread::id> worker_thread_id_;

    std::mutex worker_mutex_;
    score::concurrency::InterruptibleConditionalVariable worker_wakeup_;
    score::cpp::jthread worker_;
};

}  // namespace detail
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_SVT_CALLBACK_DISPATCHER_H
