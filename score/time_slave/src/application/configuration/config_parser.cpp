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
#include "score/time_slave/src/application/configuration/config_parser.h"

#include "score/json/json_parser.h"
#include "score/mw/log/logging.h"
#include "score/time_slave/src/common/logging_contexts.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace score
{
namespace ts
{

namespace
{

using score::json::Error;
using score::json::GetAttribute;
using score::json::MakeError;
using score::json::Object;

/// Fatal out if @p result is a type/parse error (as opposed to the caller having
/// already detected "key not present" via Object::find()).
template <typename ResultT>
void FatalOnTypeError(const ResultT& result, std::string_view key)
{
    if (!result.has_value() && result.error() != MakeError(Error::kKeyNotFound))
    {
        score::mw::log::LogFatal(kTimeSlaveAppContext)
            << "Config field '" << key << "' has wrong type: " << result.error().Message();
    }
}

/// Read a string field; leaves @p out unchanged if key is missing.
void ParseOptionalString(const Object& obj, std::string_view key, std::string& out)
{
    const auto result = GetAttribute<std::string_view>(std::cref(obj), key);
    FatalOnTypeError(result, key);
    if (result.has_value())
    {
        out = std::string{result.value()};
    }
}

/// Read an integer field; leaves @p out unchanged if key is missing.
template <typename T>
void ParseOptionalInt(const Object& obj, std::string_view key, T& out)
{
    const auto result = GetAttribute<std::int64_t>(std::cref(obj), key);
    FatalOnTypeError(result, key);
    if (result.has_value())
    {
        out = static_cast<T>(result.value());
    }
}

/// Read a bool field; leaves @p out unchanged if key is missing.
void ParseOptionalBool(const Object& obj, std::string_view key, bool& out)
{
    const auto result = GetAttribute<bool>(std::cref(obj), key);
    FatalOnTypeError(result, key);
    if (result.has_value())
    {
        out = result.value();
    }
}

/// Parse the optional "phc" nested object. If absent, @p cfg keeps defaults.
void ParsePhcConfig(const Object& obj, details::PhcConfig& cfg)
{
    const auto obj_result = GetAttribute<Object>(std::cref(obj), "phc");
    if (!obj_result.has_value())
    {
        if (obj_result.error() != MakeError(Error::kKeyNotFound))
        {
            score::mw::log::LogFatal(kTimeSlaveAppContext)
                << "Config field 'phc' is not an object: " << obj_result.error().Message();
        }
        return;
    }
    const auto& phc_obj = obj_result.value().get();

    ParseOptionalBool(phc_obj, "enabled", cfg.enabled);
    ParseOptionalString(phc_obj, "device", cfg.device);
    ParseOptionalInt(phc_obj, "step_threshold_ns", cfg.step_threshold_ns);
}

/// Parse the optional "qnx" nested object. If absent, @p qnx keeps defaults.
void ParseQnxConfig(const Object& obj, TimeSlaveConfig::QnxSettings& qnx)
{
    const auto obj_result = GetAttribute<Object>(std::cref(obj), "qnx");
    if (!obj_result.has_value())
    {
        if (obj_result.error() != MakeError(Error::kKeyNotFound))
        {
            score::mw::log::LogFatal(kTimeSlaveAppContext)
                << "Config field 'qnx' is not an object: " << obj_result.error().Message();
        }
        return;
    }
    const auto& qnx_obj = obj_result.value().get();

    ParseOptionalString(qnx_obj, "bpf_device_prefix", qnx.bpf_device_prefix);
    ParseOptionalBool(qnx_obj, "see_sent", qnx.see_sent);
}

}  // namespace

TimeSlaveConfig ParseConfig(const std::string& path)
{
    const score::json::JsonParser parser;
    auto root_result = parser.FromFile(path);
    if (!root_result.has_value())
    {
        score::mw::log::LogFatal(kTimeSlaveAppContext)
            << "Failed to parse config file '" << path << "': " << root_result.error().Message();
        return {};
    }

    auto obj_result = root_result.value().As<Object>();
    if (!obj_result.has_value())
    {
        score::mw::log::LogFatal(kTimeSlaveAppContext) << "Config file '" << path << "' root is not a JSON object";
        return {};
    }
    const auto& obj = obj_result.value().get();

    TimeSlaveConfig cfg;
    auto& opts = cfg.engine_opts;

    ParseOptionalString(obj, "iface_name", opts.iface_name);
    ParseOptionalInt(obj, "domain_number", opts.domain_number);
    ParseOptionalInt(obj, "pdelay_req_interval_ms", opts.pdelay_req_interval_ms);
    ParseOptionalInt(obj, "pdelay_warmup_ms", opts.pdelay_warmup_ms);
    ParseOptionalInt(obj, "sync_timeout_ms", opts.sync_timeout_ms);
    ParseOptionalInt(obj, "jump_future_threshold_ns", opts.jump_future_threshold_ns);
    ParseOptionalString(obj, "shm_path", cfg.shm_path);

    ParsePhcConfig(obj, opts.phc_config);
    ParseQnxConfig(obj, cfg.qnx);

    return cfg;
}

}  // namespace ts
}  // namespace score
