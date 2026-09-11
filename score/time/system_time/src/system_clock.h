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
#ifndef SCORE_TIME_SYSTEM_TIME_SRC_SYSTEM_CLOCK_H
#define SCORE_TIME_SYSTEM_TIME_SRC_SYSTEM_CLOCK_H

#include "score/time/clock/src/clock.h"
#include "score/time/clock/src/clock_snapshot.h"
#include "score/time/clock/src/no_status.h"

#include <chrono>

namespace score
{
namespace time
{

class SystemClockBackend;

/// @brief Specialization of @c ClockTraits for the std::chrono::system_clock domain.
template <>
struct ClockTraits<std::chrono::system_clock>
{
    using Backend = SystemClockBackend;
    using Duration = std::chrono::system_clock::duration;
    using Timepoint = std::chrono::system_clock::time_point;
    using Snapshot = ClockSnapshot<Timepoint, NoStatus>;

    /// @brief Obtains the current system-clock snapshot from the backend.
    static Snapshot CallNow(const Backend& impl) noexcept;
};

using SystemClock = Clock<std::chrono::system_clock>;
using SystemTimePoint = ClockTraits<std::chrono::system_clock>::Timepoint;
using SystemSnapshot = ClockTraits<std::chrono::system_clock>::Snapshot;

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_SYSTEM_TIME_SRC_SYSTEM_CLOCK_H
