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
#ifndef SCORE_TIME_SYSTEM_TIME_SRC_SYSTEM_CLOCK_BACKEND_H
#define SCORE_TIME_SYSTEM_TIME_SRC_SYSTEM_CLOCK_BACKEND_H

#include "score/time/clock/src/clock_snapshot.h"
#include "score/time/clock/src/no_status.h"

#include <score/stop_token.hpp>

#include <chrono>

namespace score
{
namespace time
{

///
/// @brief Abstract backend interface for the system-clock domain.
///
/// Concrete implementations live in SystemTime/details/.
/// This header is the contract shared by Clock<std::chrono::system_clock>,
/// test mocks, and production/test backends.
///
class SystemClockBackend
{
  public:
    virtual ~SystemClockBackend() noexcept = default;

    /// @brief Returns the current system-clock snapshot.
    virtual ClockSnapshot<std::chrono::system_clock::time_point, NoStatus> Now() const noexcept = 0;
};

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_SYSTEM_TIME_SRC_SYSTEM_CLOCK_BACKEND_H
