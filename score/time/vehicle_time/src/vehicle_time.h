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
#ifndef SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_TIME_H
#define SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_TIME_H

#include "score/time/clock/src/clock_status.h"
#include "score/time/ptp/src/pdelay_measurement_data.h"
#include "score/time/ptp/src/time_slave_sync_data.h"

#include <score/callback.hpp>

#include <chrono>
#include <cstdint>
#include <sstream>

namespace score
{
namespace time
{

// Forward declaration — VehicleTimeStatus is defined after VehicleTime below.
struct VehicleTimeStatus;

///
/// @brief Tag struct for the PTP-synchronized vehicle time domain.
///
/// Contains only domain types — no vtable, no factory, no virtual methods.
/// The implementation is hidden behind VehicleClockBackend (see VehicleTime/vehicle_clock_backend.h).
///
struct VehicleTime
{
    ///
    /// @brief Enumeration expressing the synchronisation quality state of the vehicle time domain.
    /// Each enumerator is a **bit position** (not a bitmask).
    ///
    enum class StatusFlag : std::uint8_t
    {
        kTimeOut = 0U,      /*!< TB was not synchronized within a certain time frame. */
        kSynchronized = 1U, /*!< The TB was synchronized at least once. */
        // 2U — reserved
        kTimeLeapFuture = 3U, /*!< An adjustment greater than a certain threshold has been made. */
        kTimeLeapPast = 4U,   /*!< An adjustment back in time greater than a certain threshold has been made. */
    };

    using Duration = std::chrono::nanoseconds;
    using Timepoint = std::chrono::time_point<VehicleTime, Duration>;

    static constexpr std::uint64_t kCallbackCapacity{64U};

    using TimeSlaveSyncDataReceivedCallback =
        score::cpp::callback<void(const TimeSlaveSyncData<VehicleTime>&), kCallbackCapacity>;
    using PDelayMeasurementFinishedCallback =
        score::cpp::callback<void(const PDelayMeasurementData<VehicleTime>&), kCallbackCapacity>;
    using StatusChangedCallback = score::cpp::callback<void(const VehicleTimeStatus&), kCallbackCapacity>;
};

/// @brief Formats all active VehicleTime status flags into an ostringstream for diagnostics.
template <>
std::ostringstream ClockStatus<VehicleTime::StatusFlag>::PrintTo() const;

///
/// @brief Synchronisation-quality status snapshot for the vehicle time domain.
///
/// Bundled alongside the time-point inside ClockSnapshot<VehicleTime::Timepoint, VehicleTimeStatus>.
///
struct VehicleTimeStatus
{
    /// @brief Bit-flag quality indicators for the vehicle time signal.
    ClockStatus<VehicleTime::StatusFlag> flags{};

    /// @brief Fractional rate deviation of the local clock relative to the Grand Master.
    ///        Unit: dimensionless (e.g. 1.0e-9 == 1 ppb).
    double rate_deviation{0.0};

    // Convenience delegates — avoid .flags. indirection at call sites.

    /// @brief Returns true if the vehicle time data is reliable for use right now.
    ///
    /// Reliable := kSynchronized is set AND none of {kTimeOut, kTimeLeapFuture, kTimeLeapPast} is set.
    bool IsReliable() const noexcept
    {
        return (flags.IsFlagActive(VehicleTime::StatusFlag::kSynchronized) &&
                (!flags.IsAnyOfFlagsActive({VehicleTime::StatusFlag::kTimeOut,
                                            VehicleTime::StatusFlag::kTimeLeapFuture,
                                            VehicleTime::StatusFlag::kTimeLeapPast})));
    }

    /// @brief Returns true if the vehicle time has been synchronized at least once during this lifecycle.
    ///
    /// HasBeenSynchronized := kSynchronized flag is set, regardless of any active fault flags.
    bool HasBeenSynchronized() const noexcept
    {
        return flags.IsFlagActive(VehicleTime::StatusFlag::kSynchronized);
    }

    /// @brief Returns true if the vehicle time status flags are internally consistent (no contradictory combination).
    ///
    /// Consistent := at least one flag is set AND kTimeLeapFuture and kTimeLeapPast are not both set simultaneously.
    bool IsConsistent() const noexcept
    {
        if (!flags.IsAnyOfFlagsActive({VehicleTime::StatusFlag::kTimeOut,
                                       VehicleTime::StatusFlag::kSynchronized,
                                       VehicleTime::StatusFlag::kTimeLeapFuture,
                                       VehicleTime::StatusFlag::kTimeLeapPast}))
        {
            return false;
        }
        if (flags.IsFlagActive(VehicleTime::StatusFlag::kTimeLeapFuture) &&
            flags.IsFlagActive(VehicleTime::StatusFlag::kTimeLeapPast))
        {
            return false;
        }
        return true;
    }

    /// @brief Returns the fractional rate deviation of the local clock relative to the Grand Master.
    ///        Unit: dimensionless (e.g. 1.0e-9 == 1 ppb).
    double RateDeviation() const noexcept
    {
        return rate_deviation;
    }

    /// @brief Returns true if the given StatusFlag bit-position is active.
    bool IsFlagActive(const VehicleTime::StatusFlag flag) const noexcept
    {
        return flags.IsFlagActive(flag);
    }
};

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_VEHICLE_TIME_H
