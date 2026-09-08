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
#ifndef SCORE_TIME_DAEMON_SRC_APPLICATION_TIME_DAEMON_H
#define SCORE_TIME_DAEMON_SRC_APPLICATION_TIME_DAEMON_H

#include "score/time_daemon/src/application/timebase_handler.h"

#include "score/mw/lifecycle/application.h"

namespace score
{
namespace td
{

/// @brief Main entry point for the TimeDaemon process. Orchestrates the lifecycle
/// of all daemon components.
///
/// Retrieves Vehicle Time from the PTP slave daemon, verifies and validates
/// timepoints, and distributes time to clients via shared memory. Creates
/// components in @c Initialize() and wires pub/sub relationships via
/// MessageBroker.
///
/// @see TimebaseHandler
/// @see score::mw::lifecycle::Application
class TimeDaemon final : public score::mw::lifecycle::Application
{
  public:
    explicit TimeDaemon();
    ~TimeDaemon() noexcept override = default;

    TimeDaemon(TimeDaemon&&) noexcept = delete;
    TimeDaemon(const TimeDaemon&) noexcept = delete;
    TimeDaemon& operator=(TimeDaemon&&) & noexcept = delete;
    TimeDaemon& operator=(const TimeDaemon&) & noexcept = delete;

    /// @brief Creates the MessageBroker and all machine components, and sets up subscriptions.
    ///
    /// Creates components in dependency order:
    ///   1. MessageBroker
    ///   2. ProactiveMachines: PtpMachine, ControlFlowDivider
    ///   3. ReactiveMachines: VerificationMachine, IPCMachine
    ///
    /// Wires all pub/sub relationships via MessageBroker topic configuration.
    ///
    /// @param context Application context from the middleware.
    /// @return 0 on success, non-zero on failure.
    std::int32_t Initialize(const score::mw::lifecycle::ApplicationContext& context) override;

    /// @brief Starts the ProactiveMachines and monitors the stop token.
    ///
    /// Execution loop monitors stop_token. On termination request, stops all
    /// ProactiveMachines in reverse order of startup for clean shutdown.
    ///
    /// @param token Stop token for graceful shutdown.
    /// @return 0 on success, non-zero on failure.
    std::int32_t Run(const score::cpp::stop_token& token) override;

  private:
    std::unique_ptr<TimebaseHandler> svt_timebase_handler_;
};

}  // namespace td
}  // namespace score

#endif  // SCORE_TIME_DAEMON_SRC_APPLICATION_TIME_DAEMON_H
