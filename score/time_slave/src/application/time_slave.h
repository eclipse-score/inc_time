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
#ifndef SCORE_TIME_SLAVE_SRC_APPLICATION_TIME_SLAVE_H
#define SCORE_TIME_SLAVE_SRC_APPLICATION_TIME_SLAVE_H

#include "score/time_slave/src/gptp/gptp_engine.h"
#include "score/ts_client/src/gptp_ipc_publisher.h"

#include "score/mw/lifecycle/application.h"

#include <memory>

namespace score
{
namespace ts
{

/**
 * @brief Standalone TimeSlave process that runs the gPTP engine
 *        and publishes time data to shared memory.
 *
 * TimeSlave is the gPTP protocol endpoint. It runs GptpEngine internally
 * (with RxThread + PdelayThread) and periodically writes GptpIpcData
 * to shared memory for consumption by TimeDaemon via ShmPTPEngine.
 */
class TimeSlave final : public score::mw::lifecycle::Application
{
  public:
    TimeSlave();
    ~TimeSlave() noexcept override = default;

    TimeSlave(TimeSlave&&) = delete;
    TimeSlave(const TimeSlave&) = delete;
    TimeSlave& operator=(TimeSlave&&) & = delete;
    TimeSlave& operator=(const TimeSlave&) & = delete;

    /// @brief Initializes the gPTP engine and shared memory publisher.
    /// @return 0 on success, non-zero on failure.
    std::int32_t Initialize(const score::mw::lifecycle::ApplicationContext& context) override;

    /// @brief Runs the main loop: finalizes gPTP snapshots and publishes to shared memory.
    /// @param token Stop token for graceful shutdown.
    /// @return 0 on success, non-zero on failure.
    std::int32_t Run(const score::cpp::stop_token& token) override;

  private:
    details::GptpEngineOptions opts_;
    std::unique_ptr<details::GptpEngine> engine_;
    details::GptpIpcPublisher publisher_;
};

}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_SRC_APPLICATION_TIME_SLAVE_H
