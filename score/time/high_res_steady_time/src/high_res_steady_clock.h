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
#ifndef SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_HIGH_RES_STEADY_CLOCK_H
#define SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_HIGH_RES_STEADY_CLOCK_H

#include "score/time/clock/src/clock.h"
#include "score/time/clock/src/clock_snapshot.h"
#include "score/time/clock/src/no_status.h"
#include "score/time/high_res_steady_time/src/high_res_steady_time.h"

namespace score
{
namespace time
{

class HighResSteadyClockBackend;

template <>
struct ClockTraits<HighResSteadyTime>
{
    using Backend = HighResSteadyClockBackend;
    using Duration = HighResSteadyTime::Duration;
    using Timepoint = HighResSteadyTime::Timepoint;
    using Snapshot = ClockSnapshot<Timepoint, NoStatus>;

    /// @brief Obtains the current HIRS clock snapshot from the backend.
    ///
    /// @param impl  The backend implementation to call.
    /// @return The current HIRS clock snapshot.
    static Snapshot CallNow(const Backend& impl) noexcept;
};

using HighResSteadyClock = Clock<HighResSteadyTime>;
using HighResSteadyTimePoint = ClockTraits<HighResSteadyTime>::Timepoint;
using HighResSteadySnapshot = ClockTraits<HighResSteadyTime>::Snapshot;

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_HIGH_RES_STEADY_CLOCK_H
