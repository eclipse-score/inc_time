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
#ifndef SCORE_TIME_PTP_SRC_LOCAL_PTP_DEVICE_TIMER_H
#define SCORE_TIME_PTP_SRC_LOCAL_PTP_DEVICE_TIMER_H

#include <chrono>

namespace score
{
namespace time
{

/// @brief Tag for the HW timer on the local Ethernet device where PTP messages are
///        received and which gets utilized for ingress & egress timestamping.
struct LocalPTPDeviceTimer
{
    using Duration = std::chrono::nanoseconds;
};

/// @brief Time-point type for the local PTP device timer.
using LocalPTPDeviceTimerValue = std::chrono::time_point<LocalPTPDeviceTimer, LocalPTPDeviceTimer::Duration>;

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_PTP_SRC_LOCAL_PTP_DEVICE_TIMER_H
