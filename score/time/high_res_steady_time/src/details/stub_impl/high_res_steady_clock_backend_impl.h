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
#ifndef SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_STUB_IMPL_HIGH_RES_STEADY_CLOCK_BACKEND_IMPL_H
#define SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_STUB_IMPL_HIGH_RES_STEADY_CLOCK_BACKEND_IMPL_H

// Internal header — include ONLY from stub_impl/high_res_steady_clock_backend_impl.cpp.

#include "score/time/clock/src/no_status.h"
#include "score/time/high_res_steady_time/src/high_res_steady_clock_backend.h"

#include <chrono>

namespace score
{
namespace time
{
namespace detail
{

/// @brief Stub backend for the HIRS clock domain (host/test-only).
///
/// Wraps std::chrono::steady_clock so that Clock<HighResSteadyTime>::GetInstance()
/// resolves at link time even when ScopedClockOverride is not active.
class HighResSteadyClockBackendImpl final : public HighResSteadyClockBackend
{
  public:
    HighResSteadyClockBackendImpl() noexcept = default;
    ~HighResSteadyClockBackendImpl() noexcept override = default;
    HighResSteadyClockBackendImpl(const HighResSteadyClockBackendImpl&) = delete;
    HighResSteadyClockBackendImpl& operator=(const HighResSteadyClockBackendImpl&) = delete;
    HighResSteadyClockBackendImpl(HighResSteadyClockBackendImpl&&) = delete;
    HighResSteadyClockBackendImpl& operator=(HighResSteadyClockBackendImpl&&) = delete;

    /// @brief Returns the current HIRS snapshot from std::chrono::steady_clock.
    ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus> Now() const noexcept override
    {
        const auto raw = std::chrono::steady_clock::now().time_since_epoch();
        const HighResSteadyTime::Timepoint tp{std::chrono::duration_cast<HighResSteadyTime::Duration>(raw)};
        return ClockSnapshot<HighResSteadyTime::Timepoint, NoStatus>{tp, NoStatus{}};
    }
};

}  // namespace detail
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_STUB_IMPL_HIGH_RES_STEADY_CLOCK_BACKEND_IMPL_H
