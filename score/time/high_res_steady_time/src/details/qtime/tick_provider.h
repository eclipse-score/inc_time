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
#ifndef SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_QTIME_TICK_PROVIDER_H
#define SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_QTIME_TICK_PROVIDER_H

#include <cstdint>

namespace score
{
namespace time
{
namespace high_res_steady_time
{
namespace qtime
{

/// @brief Returns the number of hardware clock cycles per second from the QNX syspage.
///
/// Extracted as a free function so it can be replaced by a mock in unit tests.
std::uint64_t GetClockCyclesPerSec();

}  // namespace qtime
}  // namespace high_res_steady_time
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_QTIME_TICK_PROVIDER_H
