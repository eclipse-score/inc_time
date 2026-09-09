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
#ifndef SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_CLOCK_BACKEND_H
#define SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_CLOCK_BACKEND_H

#include "score/time/clock/src/clock_snapshot.h"
#include "score/time/vehicle_time/src/vehicle_time.h"

#include <score/stop_token.hpp>

#include <chrono>

namespace score
{
namespace time
{

///
/// \brief Pure-virtual pimpl interface for the vehicle time domain backend.
///
/// INTERNAL — must not be included by user code.
/// Consumers: test mocks and backend implementations under details/.
///
class VehicleClockBackend
{
  public:
    virtual ~VehicleClockBackend() noexcept = default;

    /// \brief Returns the current vehicle time snapshot (time-point + quality status).
    virtual ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus> Now() const noexcept = 0;

    /// \brief Initialises the backend resource.
    ///
    /// Idempotent: a second call on an already-initialised backend returns \c true immediately.
    ///
    /// \return \c true on success; \c false on failure.
    virtual bool Init() noexcept = 0;

    /// \brief Returns true if the vehicle time backend resource is available.
    virtual bool IsAvailable() const noexcept = 0;

    /// \brief Blocks until the vehicle time resource is available or the deadline / stop-token fires.
    ///
    /// \param token  Stop token that can interrupt the wait.
    /// \param until  Steady-clock deadline after which the wait is abandoned.
    ///
    /// \return true if the resource became available, false if the wait was aborted.
    virtual bool WaitUntilAvailable(const score::cpp::stop_token& token,
                                    std::chrono::steady_clock::time_point until) const noexcept = 0;

    /// \brief Installs the callback invoked when new time-sync data arrives.
    ///
    /// Fires once for every new Sync/Follow-Up frame received from the TimeDaemon after
    /// registration.  Invoked on the backend's dedicated worker thread.
    ///
    /// Replacing an installed callback is safe while an invocation is in flight: the call
    /// returns only once the previous callback is no longer running (unless made from
    /// within that callback itself).
    virtual void SetTimeSlaveSyncDataReceivedCallback(
        VehicleTime::TimeSlaveSyncDataReceivedCallback&& callback) noexcept = 0;

    /// \brief Removes the time-sync data callback.
    ///
    /// Returns only once an in-flight invocation has completed (unless called from within
    /// the callback itself), so callers may release captured resources afterwards.
    virtual void UnsetTimeSlaveSyncDataReceivedCallback() noexcept = 0;

    /// \brief Installs the callback invoked after a finished pDelay measurement.
    ///
    /// Fires once for every new pDelay measurement result received from the TimeDaemon after
    /// registration.  Invoked on the backend's dedicated worker thread.  Same replacement
    /// guarantees as \c SetTimeSlaveSyncDataReceivedCallback().
    virtual void SetPDelayMeasurementFinishedCallback(
        VehicleTime::PDelayMeasurementFinishedCallback&& callback) noexcept = 0;

    /// \brief Removes the pDelay measurement callback.
    ///
    /// Same completion guarantee as \c UnsetTimeSlaveSyncDataReceivedCallback().
    virtual void UnsetPDelayMeasurementFinishedCallback() noexcept = 0;

    /// \brief Installs the callback invoked when VehicleTimeStatus flags change.
    ///
    /// The callback fires:
    ///  - unconditionally on the first status frame received after registration; and
    ///  - afterwards only when the status flags differ from the last delivered value
    ///    (rate deviation is ignored for comparison).
    ///
    /// The callback is invoked on the backend's dedicated worker thread — the callback
    /// implementation must be thread-safe.  Same replacement guarantees as
    /// \c SetTimeSlaveSyncDataReceivedCallback().
    virtual void SetStatusChangedCallback(VehicleTime::StatusChangedCallback&& callback) noexcept = 0;

    /// \brief Removes the status-changed callback.
    ///
    /// Same completion guarantee as \c UnsetTimeSlaveSyncDataReceivedCallback().
    virtual void UnsetStatusChangedCallback() noexcept = 0;
};

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_CLOCK_BACKEND_H
