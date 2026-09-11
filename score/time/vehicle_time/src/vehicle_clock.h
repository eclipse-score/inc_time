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
#ifndef SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_CLOCK_H
#define SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_CLOCK_H

#include "score/time/clock/src/availability_hook.h"
#include "score/time/clock/src/clock.h"
#include "score/time/clock/src/clock_snapshot.h"
#include "score/time/clock/src/clock_status.h"
#include "score/time/clock/src/initialization_hook.h"
#include "score/time/ptp/src/pdelay_measurement_data.h"
#include "score/time/ptp/src/time_slave_sync_data.h"
#include "score/time/vehicle_time/src/vehicle_time.h"

#include <score/stop_token.hpp>

#include <chrono>

namespace score
{
namespace time
{

class VehicleClockBackend;

template <>
struct ClockTraits<VehicleTime>
{
    using Backend = VehicleClockBackend;
    using Duration = VehicleTime::Duration;
    using Timepoint = VehicleTime::Timepoint;
    using Snapshot = ClockSnapshot<Timepoint, VehicleTimeStatus>;

    /// @brief Obtains the current vehicle time snapshot from the backend.
    static Snapshot CallNow(const Backend& impl) noexcept;
};

template <>
struct InitializationHook<VehicleTime>
{
    using Backend = ClockTraits<VehicleTime>::Backend;

    /// @brief Delegates to \c VehicleClockBackend::Init().
    static bool CallInit(Backend& impl) noexcept;
};

template <>
struct AvailabilityHook<VehicleTime>
{
    using Backend = ClockTraits<VehicleTime>::Backend;

    /// @brief Returns true if the vehicle time backend resource is available.
    static bool CallIsAvailable(const Backend& impl) noexcept;

    /// @brief Blocks until the vehicle time resource is available or deadline / stop-token fires.
    static bool CallWaitUntilAvailable(const Backend& impl,
                                       const score::cpp::stop_token& token,
                                       std::chrono::steady_clock::time_point until) noexcept;
};

template <>
struct SubscriptionHook<VehicleTime, TimeSlaveSyncData<VehicleTime>>
{
    using Backend = ClockTraits<VehicleTime>::Backend;
    using Callback = VehicleTime::TimeSlaveSyncDataReceivedCallback;

    static void Subscribe(Backend& impl, Callback cb) noexcept;
    static void Unsubscribe(Backend& impl) noexcept;
};

template <>
struct SubscriptionHook<VehicleTime, PDelayMeasurementData<VehicleTime>>
{
    using Backend = ClockTraits<VehicleTime>::Backend;
    using Callback = VehicleTime::PDelayMeasurementFinishedCallback;

    static void Subscribe(Backend& impl, Callback cb) noexcept;
    static void Unsubscribe(Backend& impl) noexcept;
};

template <>
struct SubscriptionHook<VehicleTime, VehicleTimeStatus>
{
    using Backend = ClockTraits<VehicleTime>::Backend;
    using Callback = VehicleTime::StatusChangedCallback;

    static void Subscribe(Backend& impl, Callback cb) noexcept;
    static void Unsubscribe(Backend& impl) noexcept;
};

using VehicleClock = Clock<VehicleTime>;
using VehicleTimePoint = ClockTraits<VehicleTime>::Timepoint;
using VehicleSnapshot = ClockTraits<VehicleTime>::Snapshot;

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_CLOCK_H
