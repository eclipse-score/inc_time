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

#include "score/time/high_res_steady_time/src/high_res_steady_clock.h"
#include "score/time/vehicle_time/src/vehicle_clock.h"
#include "score/time/vehicle_time/src/vehicle_clock_backend.h"
#include "score/time_daemon/src/ipc/svt/receiver/svt_receiver.h"

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
/// Test backend is separate stub implementation used only by tests.
///
/// Implements @c VehicleClockBackend by reading live PTP data from the TimeDaemon
/// via @c score::td::SvtReceiver.  The adjusted vehicle time is computed as:
///
///   adjusted_ptp = ptp_stamp_at_capture + (local_now - local_at_capture)
///
/// where the local reference clock is supplied via @c HighResSteadyClock::GetInstance()
/// (captured once at construction to avoid per-call mutex overhead).
///
/// Callback registration is not yet supported; all Set/Unset methods are no-ops
/// until the TimeDaemon IPC layer provides a subscription facility.
///
/// @note Placed in @c score::time::detail (rather than an anonymous namespace) so
/// that vehicle_clock_backend_impl_test.cpp can construct it directly with injected mocks.
/// Only one backend translation unit must be linked per binary to avoid ODR issues.
class VehicleClockBackendImpl final : public VehicleClockBackend
{
  public:
    /// @brief Constructs backend with injected TimeDaemon receiver and local steady clock.
    ///
    /// @param receiver    SvtReceiver instance used to read TimeDaemon SVT samples.
    /// @param local_clock Local steady clock used for PTP-to-now extrapolation.
    VehicleClockBackendImpl(std::shared_ptr<score::td::SvtReceiver> receiver, HighResSteadyClock local_clock) noexcept;

    ~VehicleClockBackendImpl() noexcept override = default;
    VehicleClockBackendImpl(const VehicleClockBackendImpl&) = delete;
    VehicleClockBackendImpl& operator=(const VehicleClockBackendImpl&) = delete;
    VehicleClockBackendImpl(VehicleClockBackendImpl&&) = delete;
    VehicleClockBackendImpl& operator=(VehicleClockBackendImpl&&) = delete;

    /// @brief Returns vehicle time snapshot.
    ///
    /// Returns empty snapshot when backend is not initialised, receiver has no
    /// sample, or local reference time is behind sample capture point.
    ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus> Now() const noexcept override;

    /// @brief Initializes TimeDaemon SVT receiver once (thread-safe, idempotent).
    ///
    /// Uses double-checked locking around @c SvtReceiver::Init() and stores
    /// resulting readiness in @c is_ready_.
    bool Init() noexcept override;

    /// @brief Returns cached readiness state set by @c Init().
    bool IsAvailable() const noexcept override;

    /// @brief Polls readiness until available, stop requested, or deadline reached.
    ///
    /// Poll interval is 10 ms.
    bool WaitUntilAvailable(const score::cpp::stop_token& token,
                            std::chrono::steady_clock::time_point until) const noexcept override;

    /// @brief No-op in production backend until callback transport support is available.
    void SetTimeSlaveSyncDataReceivedCallback(
        VehicleTime::TimeSlaveSyncDataReceivedCallback&& callback) noexcept override;

    /// @brief No-op in production backend until callback transport support is available.
    void UnsetTimeSlaveSyncDataReceivedCallback() noexcept override;

    /// @brief No-op in production backend until callback transport support is available.
    void SetPDelayMeasurementFinishedCallback(
        VehicleTime::PDelayMeasurementFinishedCallback&& callback) noexcept override;

    /// @brief No-op in production backend until callback transport support is available.
    void UnsetPDelayMeasurementFinishedCallback() noexcept override;

    /// @brief No-op in production backend until callback transport support is available.
    void SetStatusChangedCallback(VehicleTime::StatusChangedCallback&& callback) noexcept override;

    /// @brief No-op in production backend until callback transport support is available.
    void UnsetStatusChangedCallback() noexcept override;

  private:
    /// @brief Converts PTP status flags from the TimeDaemon IPC representation to
    ///        the @c ClockStatus<VehicleTime::StatusFlag> representation.
    static ClockStatus<VehicleTime::StatusFlag> ConvertPtpStatus(
        const score::td::svt::TimeBaseStatus& ptp_status) noexcept;

    std::atomic_bool is_ready_;
    std::mutex init_mutex_;
    std::shared_ptr<score::td::SvtReceiver> svt_receiver_;
    HighResSteadyClock local_clock_;
};

}  // namespace detail
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_VEHICLE_CLOCK_BACKEND_IMPL_H
