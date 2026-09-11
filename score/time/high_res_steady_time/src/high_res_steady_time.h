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
#ifndef SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_HIGH_RES_STEADY_TIME_H
#define SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_HIGH_RES_STEADY_TIME_H

#include <chrono>

namespace score
{
namespace time
{

///
/// @brief Tag struct for the High-Resolution Steady (HIRS) time domain.
struct HighResSteadyTime
{
    using Duration = std::chrono::nanoseconds;
    using Timepoint = std::chrono::time_point<HighResSteadyTime, Duration>;
};

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_HIGH_RES_STEADY_TIME_H
