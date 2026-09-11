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
#ifndef SCORE_TIME_STEADY_TIME_SRC_STEADY_CLOCK_H
#define SCORE_TIME_STEADY_TIME_SRC_STEADY_CLOCK_H

#include "score/time/clock/src/clock.h"
#include "score/time/clock/src/clock_snapshot.h"
#include "score/time/clock/src/no_status.h"

#include <chrono>

namespace score
{
namespace time
{

class SteadyClockBackend;

/// @brief Specialization of @c ClockTraits for the std::chrono::steady_clock domain.
template <>
struct ClockTraits<std::chrono::steady_clock>
{
    using Backend = SteadyClockBackend;
    using Duration = std::chrono::steady_clock::duration;
    using Timepoint = std::chrono::steady_clock::time_point;
    using Snapshot = ClockSnapshot<Timepoint, NoStatus>;

    /// @brief Obtains the current steady-clock snapshot from the backend.
    static Snapshot CallNow(const Backend& impl) noexcept;
};

using SteadyClock = Clock<std::chrono::steady_clock>;
using SteadyTimePoint = ClockTraits<std::chrono::steady_clock>::Timepoint;
using SteadySnapshot = ClockTraits<std::chrono::steady_clock>::Snapshot;

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_STEADY_TIME_SRC_STEADY_CLOCK_H
