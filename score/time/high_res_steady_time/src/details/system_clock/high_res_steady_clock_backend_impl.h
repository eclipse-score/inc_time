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
#ifndef SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_SYSTEM_CLOCK_HIGH_RES_STEADY_CLOCK_BACKEND_IMPL_H
#define SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_SYSTEM_CLOCK_HIGH_RES_STEADY_CLOCK_BACKEND_IMPL_H

#include "score/time/clock/src/no_status.h"
#include "score/time/high_res_steady_time/src/high_res_steady_clock_backend.h"

namespace score
{
namespace time
{
namespace high_res_steady_time
{
namespace sys_time
{

/// @brief Production HighResSteadyTime backend for non-QNX platforms.
///
/// Reads the current time from @c std::chrono::high_resolution_clock and
/// converts it to an @c HighResSteadyTime::Timepoint.
class HighResSteadyClockBackendImpl final : public HighResSteadyClockBackend
{
  public:
    HighResSteadyClockBackendImpl() noexcept;
    HighResSteadyClockBackendImpl(const HighResSteadyClockBackendImpl&) noexcept = delete;
    HighResSteadyClockBackendImpl& operator=(const HighResSteadyClockBackendImpl&) noexcept = delete;
    HighResSteadyClockBackendImpl(HighResSteadyClockBackendImpl&&) noexcept = delete;
    HighResSteadyClockBackendImpl& operator=(HighResSteadyClockBackendImpl&&) noexcept = delete;
    ~HighResSteadyClockBackendImpl() noexcept override;

    /// @brief Returns the current HIRS snapshot using @c high_resolution_clock.
    ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus> Now() const noexcept override;
};

}  // namespace sys_time
}  // namespace high_res_steady_time
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_SYSTEM_CLOCK_HIGH_RES_STEADY_CLOCK_BACKEND_IMPL_H
