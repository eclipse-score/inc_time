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

#ifndef SCORE_TIME_CLOCK_SRC_CLOCK_SNAPSHOT_H
#define SCORE_TIME_CLOCK_SRC_CLOCK_SNAPSHOT_H

#include <chrono>

namespace score
{
namespace time
{

/// @brief Immutable snapshot of a clock reading at one instant, bundled with its quality metadata.
///
/// Returned by @c Clock<Tag>::Now(), capturing the time-point and status together.
/// Immutability prevents time-of-check-time-of-use bugs where separate @c GetTime() / @c GetStatus()
/// calls could observe different backend snapshots.
///
/// @tparam TimepointT  The time-point type (e.g. VehicleTime::Timepoint or
///                     std::chrono::steady_clock::time_point).
/// @tparam StatusT     The status type that accompanies the reading (e.g. VehicleTimeStatus
///                     or NoStatus for clocks with no quality concept).
template <typename TimepointT, typename StatusT>
class ClockSnapshot
{
  public:
    /// @brief Default constructor. Value-initializes both fields.
    ClockSnapshot() = default;

    /// @brief Explicit constructor.
    ///
    /// @param tp  The time-point value.
    /// @param st  The status value.
    explicit ClockSnapshot(TimepointT tp, StatusT st) noexcept : time_point_{tp}, status_{st} {}

    /// @brief Returns the time-point obtained from the clock.
    TimepointT TimePoint() const noexcept
    {
        return time_point_;
    }

    /// @brief Returns the quality status associated with the reading.
    StatusT Status() const noexcept
    {
        return status_;
    }

    /// @brief Returns the duration elapsed since the clock's epoch in the clock's native resolution.
    typename TimepointT::duration TimeSinceEpoch() const noexcept
    {
        return time_point_.time_since_epoch();
    }

    /// @brief Returns the duration elapsed since the clock's epoch, converted to nanoseconds.
    std::chrono::nanoseconds TimePointNs() const noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(TimeSinceEpoch());
    }

  private:
    /// @brief The time-point obtained from the clock.
    TimepointT time_point_{};

    /// @brief The quality status associated with the reading.
    StatusT status_{};
};

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_CLOCK_SRC_CLOCK_SNAPSHOT_H
