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
#ifndef SCORE_TIME_SYSTEM_TIME_SRC_DETAILS_STUB_IMPL_SYSTEM_CLOCK_BACKEND_IMPL_H
#define SCORE_TIME_SYSTEM_TIME_SRC_DETAILS_STUB_IMPL_SYSTEM_CLOCK_BACKEND_IMPL_H

// Internal header — include ONLY from stub_impl/system_clock_backend_impl.cpp.

#include "score/time/clock/src/no_status.h"
#include "score/time/system_time/src/system_clock_backend.h"

#include <chrono>

namespace score
{
namespace time
{
namespace detail
{

/// @brief Stub backend for the system-clock domain (host/test-only).
///
/// Returns a default-constructed (epoch-zero) snapshot for all calls so that
/// tests without an active ScopedClockOverride get a deterministic, safe value.
class SystemClockBackendImpl final : public SystemClockBackend
{
  public:
    SystemClockBackendImpl() noexcept = default;
    ~SystemClockBackendImpl() noexcept override = default;
    SystemClockBackendImpl(const SystemClockBackendImpl&) = delete;
    SystemClockBackendImpl& operator=(const SystemClockBackendImpl&) = delete;
    SystemClockBackendImpl(SystemClockBackendImpl&&) = delete;
    SystemClockBackendImpl& operator=(SystemClockBackendImpl&&) = delete;

    /// @brief Returns the current system-clock snapshot.
    ClockSnapshot<std::chrono::system_clock::time_point, NoStatus> Now() const noexcept override
    {
        return ClockSnapshot<std::chrono::system_clock::time_point, NoStatus>{};
    }
};

}  // namespace detail
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_SYSTEM_TIME_SRC_DETAILS_STUB_IMPL_SYSTEM_CLOCK_BACKEND_IMPL_H
