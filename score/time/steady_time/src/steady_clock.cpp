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
#include "score/time/steady_time/src/steady_clock.h"
#include "score/time/steady_time/src/steady_clock_backend.h"

namespace score
{
namespace time
{

// # req-Id: comp_req__steady_time__snapshot
ClockTraits<std::chrono::steady_clock>::Snapshot ClockTraits<std::chrono::steady_clock>::CallNow(
    const Backend& impl) noexcept
{
    return impl.Now();
}

}  // namespace time
}  // namespace score
