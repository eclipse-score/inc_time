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
#include "score/time_slave/src/application/time_slave.h"

#include "score/mw/log/logging.h"
#include "score/time_slave/src/application/configuration/config_parser.h"
#include "score/time_slave/src/common/logging_contexts.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>

namespace score
{
namespace ts
{

namespace
{

constexpr std::int32_t kInitSuccess = 0;
constexpr std::int32_t kInitFailure = -1;

}  // namespace

TimeSlave::TimeSlave() = default;

std::int32_t TimeSlave::Initialize(const score::mw::lifecycle::ApplicationContext& context)
{
    namespace fs = std::filesystem;

    // Resolve config path: --config CLI > TIMESLAVE_CONFIG env > ./etc/time_slave_config.json
    fs::path config_path;
    const std::string cli_config = context.get_argument("--config");
    if (!cli_config.empty())
    {
        config_path = cli_config;
    }
    else if (const char* env_config = std::getenv("TIMESLAVE_CONFIG"); env_config != nullptr && env_config[0] != '\0')
    {
        config_path = env_config;
    }
    else
    {
        config_path = "./etc/time_slave_config.json";
    }

    TimeSlaveConfig cfg;
    if (fs::exists(config_path))
    {
        cfg = ParseConfig(config_path.string());
        opts_ = cfg.engine_opts;
        score::mw::log::LogInfo(kTimeSlaveAppContext)
            << "Loaded config from " << config_path.string() << " (iface=" << opts_.iface_name
            << ", phc=" << (opts_.phc_config.enabled ? std::string_view{"enabled"} : std::string_view{"disabled"})
            << ")";
    }
    else
    {
        score::mw::log::LogInfo(kTimeSlaveAppContext)
            << "No config file found at " << config_path.string() << ", using built-in defaults";
    }

    // Legacy env-var override: GPTP_IFACE overrides iface_name.
    if (const char* iface_env = std::getenv("GPTP_IFACE"); iface_env != nullptr && iface_env[0] != '\0')
    {
        opts_.iface_name = iface_env;
        score::mw::log::LogInfo(kTimeSlaveAppContext) << "Using interface from GPTP_IFACE: " << opts_.iface_name;
    }

    // Apply QNX-specific settings via environment variables (read by the QNX
    // raw-socket shim). Pre-existing env vars are never overridden so users can
    // still override at the command line.
    if (!cfg.qnx.bpf_device_prefix.empty() && std::getenv("SOCK") == nullptr)
    {
        ::setenv("SOCK", cfg.qnx.bpf_device_prefix.c_str(), 0);
    }
    if (cfg.qnx.see_sent && std::getenv("QNX_RAW_SEESENT") == nullptr)
    {
        ::setenv("QNX_RAW_SEESENT", "1", 0);
    }

    engine_ = std::make_unique<details::GptpEngine>(opts_);

    if (!engine_->Initialize())
    {
        score::mw::log::LogError(kTimeSlaveAppContext) << "TimeSlave: GptpEngine initialization failed";
        return kInitFailure;
    }

    if (!publisher_.Open())
    {
        score::mw::log::LogError(kTimeSlaveAppContext) << "TimeSlave: shared memory publisher initialization failed";
        return kInitFailure;
    }

    score::mw::log::LogInfo(kTimeSlaveAppContext) << "TimeSlave initialized";
    return kInitSuccess;
}

std::int32_t TimeSlave::Run(const score::cpp::stop_token& token)
{
    constexpr auto kPublishInterval = std::chrono::milliseconds{50};

    score::mw::log::LogInfo(kTimeSlaveAppContext) << "TimeSlave running";

    while (!token.stop_requested())
    {
        engine_->FinalizeSnapshot();
        score::ts::GptpIpcData data{};
        if (engine_->ReadPTPSnapshot(data))
        {
            publisher_.Publish(data);
        }

        std::this_thread::sleep_for(kPublishInterval);
    }

    engine_->Deinitialize();
    publisher_.Close();

    score::mw::log::LogInfo(kTimeSlaveAppContext) << "TimeSlave stopped";
    return kInitSuccess;
}

}  // namespace ts
}  // namespace score
