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
#ifndef SCORE_TIME_STEADY_TIME_SRC_DETAILS_STUB_IMPL_STEADY_CLOCK_BACKEND_IMPL_H
#define SCORE_TIME_STEADY_TIME_SRC_DETAILS_STUB_IMPL_STEADY_CLOCK_BACKEND_IMPL_H

// Internal header — include ONLY from stub_impl/steady_clock_backend_impl.cpp.

#include "score/time/clock/src/no_status.h"
#include "score/time/steady_time/src/steady_clock_backend.h"

#include <chrono>

namespace score
{
namespace time
{
namespace detail
{

/// @brief Stub backend for the steady-clock domain (host/test-only).
///
/// Returns a default-constructed (epoch-zero) snapshot for all calls so that
/// tests without an active ScopedClockOverride get a deterministic, safe value.
class SteadyClockBackendImpl final : public SteadyClockBackend
{
  public:
    SteadyClockBackendImpl() noexcept = default;
    ~SteadyClockBackendImpl() noexcept override = default;
    SteadyClockBackendImpl(const SteadyClockBackendImpl&) = delete;
    SteadyClockBackendImpl& operator=(const SteadyClockBackendImpl&) = delete;
    SteadyClockBackendImpl(SteadyClockBackendImpl&&) = delete;
    SteadyClockBackendImpl& operator=(SteadyClockBackendImpl&&) = delete;

    /// @brief Returns the current steady-clock snapshot for all calls.
    ClockSnapshot<std::chrono::steady_clock::time_point, NoStatus> Now() const noexcept override
    {
        return ClockSnapshot<std::chrono::steady_clock::time_point, NoStatus>{};
    }
};

}  // namespace detail
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_STEADY_TIME_SRC_DETAILS_STUB_IMPL_STEADY_CLOCK_BACKEND_IMPL_H
