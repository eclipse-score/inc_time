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
#ifndef SCORE_TIME_PTP_SRC_MASTER_PTP_DEVICE_TIMER_H
#define SCORE_TIME_PTP_SRC_MASTER_PTP_DEVICE_TIMER_H

#include <chrono>

namespace score
{
namespace time
{

/// @brief Tag for the HW timer on the PTP time master's Ethernet device where the
///        PTP client's pDelay measurement request packets are received and the
///        corresponding reply packets will be sent.
struct MasterPTPDeviceTimer
{
    using Duration = std::chrono::nanoseconds;
};

/// @brief Time-point type for the master PTP device timer.
using MasterPTPDeviceTimerValue = std::chrono::time_point<MasterPTPDeviceTimer, MasterPTPDeviceTimer::Duration>;

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_PTP_SRC_MASTER_PTP_DEVICE_TIMER_H
