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
#ifndef SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_CLOCK_UTIL_H
#define SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_CLOCK_UTIL_H

#include "score/time_slave/src/gptp/details/ptp_types.h"

#include <time.h>
#include <cstdint>

namespace score
{
namespace ts
{
namespace details
{

/// @brief Returns current @c CLOCK_MONOTONIC time in nanoseconds.
///
/// @details
/// `CLOCK_MONOTONIC` is a non-decreasing system uptime-style clock and is not
/// tied to wall-clock time.
/// - Linux: starts at an unspecified point (typically boot) and is not affected
///   by manual/NTP wall-clock adjustments; it may still be slightly slewed.
/// - QNX/POSIX systems: monotonic clock with an unspecified epoch, intended for
///   interval measurement and not affected by wall-clock set operations.
///
/// Use this value only for elapsed-time calculations, not for calendar time.
///
/// @return Nanoseconds since an unspecified epoch, or 0 on failure.
inline std::int64_t MonoNs() noexcept
{
    ::timespec ts{};
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return static_cast<std::int64_t>(ts.tv_sec) * kNsPerSec + ts.tv_nsec;
}

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_CLOCK_UTIL_H
