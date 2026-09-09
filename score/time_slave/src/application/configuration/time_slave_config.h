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
#ifndef SCORE_TIME_SLAVE_CONFIGURATION_TIME_SLAVE_CONFIG_H
#define SCORE_TIME_SLAVE_CONFIGURATION_TIME_SLAVE_CONFIG_H

#include "score/time_slave/src/gptp/gptp_engine.h"
#include "score/time_slave/src/gptp/phc/phc_adjuster.h"

#include <cstdint>
#include <string>

namespace score
{
namespace ts
{

/**
 * @brief Runtime configuration for the TimeSlave process.
 *
 * Loaded from a JSON file (or filled in with defaults when no file is
 * provided). Holds both the GptpEngine/PHC options and any process-level
 * configuration.
 */
struct TimeSlaveConfig
{
    /// gPTP engine options (passed directly to GptpEngine).
    details::GptpEngineOptions engine_opts{};

    /// Path to shared-memory segment used to publish PTP snapshots.
    std::string shm_path = "/gptp_shmem";

    /// QNX-specific settings. Only relevant on QNX builds.
    struct QnxSettings
    {
        std::string bpf_device_prefix = "/dev/bpf";  ///< BPF device path prefix.
        bool see_sent = false;                       ///< Deliver sent frames to RX BPF.
    };

    QnxSettings qnx{};
};

}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_CONFIGURATION_TIME_SLAVE_CONFIG_H
