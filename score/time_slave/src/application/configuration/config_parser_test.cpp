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

#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace score
{
namespace ts
{
namespace
{

using ::testing::TestWithParam;
using ::testing::ValuesIn;

/// Write @p content to a temp file and return its path.
std::string WriteTempFile(const std::string& name, const std::string& content)
{
    const std::string path = testing::TempDir() + name;
    std::ofstream ofs(path);
    ofs << content;
    return path;
}

TEST(ConfigParserTest, EmptyObjectReturnsDefaults)
{
    const auto path = WriteTempFile("empty_config.json", "{}");
    const auto cfg = ParseConfig(path);

    EXPECT_EQ(cfg.engine_opts.iface_name, "emac0");
    EXPECT_EQ(cfg.engine_opts.domain_number, 0);
    EXPECT_EQ(cfg.engine_opts.pdelay_req_interval_ms, 1000);
    EXPECT_EQ(cfg.engine_opts.pdelay_warmup_ms, 2000);
    EXPECT_EQ(cfg.engine_opts.sync_timeout_ms, 3300);
    EXPECT_EQ(cfg.engine_opts.jump_future_threshold_ns, 500'000'000LL);
    EXPECT_EQ(cfg.shm_path, "/gptp_shmem");
    EXPECT_FALSE(cfg.engine_opts.phc_config.enabled);
    EXPECT_EQ(cfg.qnx.bpf_device_prefix, "/dev/bpf");
    EXPECT_FALSE(cfg.qnx.see_sent);
}

TEST(ConfigParserTest, ParseAllTopLevelFields)
{
    const std::string json = R"({
        "iface_name": "eth1",
        "domain_number": 5,
        "pdelay_req_interval_ms": 500,
        "pdelay_warmup_ms": 1000,
        "sync_timeout_ms": 2000,
        "jump_future_threshold_ns": 100000000,
        "shm_path": "/custom_shm"
    })";
    const auto path = WriteTempFile("full_config.json", json);
    const auto cfg = ParseConfig(path);

    EXPECT_EQ(cfg.engine_opts.iface_name, "eth1");
    EXPECT_EQ(cfg.engine_opts.domain_number, 5);
    EXPECT_EQ(cfg.engine_opts.pdelay_req_interval_ms, 500);
    EXPECT_EQ(cfg.engine_opts.pdelay_warmup_ms, 1000);
    EXPECT_EQ(cfg.engine_opts.sync_timeout_ms, 2000);
    EXPECT_EQ(cfg.engine_opts.jump_future_threshold_ns, 100'000'000LL);
    EXPECT_EQ(cfg.shm_path, "/custom_shm");
}

TEST(ConfigParserTest, ParsePhcConfig)
{
    const std::string json = R"({
        "phc": {
            "enabled": true,
            "device": "ptp0",
            "step_threshold_ns": 50000000
        }
    })";
    const auto path = WriteTempFile("phc_config.json", json);
    const auto cfg = ParseConfig(path);

    EXPECT_TRUE(cfg.engine_opts.phc_config.enabled);
    EXPECT_EQ(cfg.engine_opts.phc_config.device, "ptp0");
    EXPECT_EQ(cfg.engine_opts.phc_config.step_threshold_ns, 50'000'000);
}

TEST(ConfigParserTest, ParseQnxConfig)
{
    const std::string json = R"({
        "qnx": {
            "bpf_device_prefix": "/dev/bpf_test",
            "see_sent": true
        }
    })";
    const auto path = WriteTempFile("qnx_config.json", json);
    const auto cfg = ParseConfig(path);

    EXPECT_EQ(cfg.qnx.bpf_device_prefix, "/dev/bpf_test");
    EXPECT_TRUE(cfg.qnx.see_sent);
}

TEST(ConfigParserTest, ParsePartialFields)
{
    const std::string json = R"({
        "iface_name": "eth2"
    })";
    const auto path = WriteTempFile("partial_config.json", json);
    const auto cfg = ParseConfig(path);

    // Only iface_name changed; everything else keeps default.
    EXPECT_EQ(cfg.engine_opts.iface_name, "eth2");
    EXPECT_EQ(cfg.engine_opts.domain_number, 0);
    EXPECT_EQ(cfg.engine_opts.pdelay_req_interval_ms, 1000);
    EXPECT_EQ(cfg.shm_path, "/gptp_shmem");
    EXPECT_FALSE(cfg.engine_opts.phc_config.enabled);
    EXPECT_FALSE(cfg.qnx.see_sent);
}

TEST(ConfigParserTest, FullExampleConfigRoundTrip)
{
    const auto path = WriteTempFile("example.json",
                                    "{\n"
                                    "    \"iface_name\": \"emac0\",\n"
                                    "    \"domain_number\": 0,\n"
                                    "    \"pdelay_req_interval_ms\": 1000,\n"
                                    "    \"pdelay_warmup_ms\": 2000,\n"
                                    "    \"sync_timeout_ms\": 3300,\n"
                                    "    \"jump_future_threshold_ns\": 500000000,\n"
                                    "    \"shm_path\": \"/gptp_shmem\",\n"
                                    "    \"phc\": {\n"
                                    "        \"enabled\": true,\n"
                                    "        \"device\": \"emac0\",\n"
                                    "        \"step_threshold_ns\": 100000000\n"
                                    "    },\n"
                                    "    \"qnx\": {\n"
                                    "        \"bpf_device_prefix\": \"/dev/bpf\",\n"
                                    "        \"see_sent\": true\n"
                                    "    }\n"
                                    "}\n");

    const auto cfg = ParseConfig(path);

    EXPECT_EQ(cfg.engine_opts.iface_name, "emac0");
    EXPECT_EQ(cfg.engine_opts.domain_number, 0);
    EXPECT_EQ(cfg.engine_opts.pdelay_req_interval_ms, 1000);
    EXPECT_EQ(cfg.engine_opts.pdelay_warmup_ms, 2000);
    EXPECT_EQ(cfg.engine_opts.sync_timeout_ms, 3300);
    EXPECT_EQ(cfg.engine_opts.jump_future_threshold_ns, 500'000'000LL);
    EXPECT_EQ(cfg.shm_path, "/gptp_shmem");
    EXPECT_TRUE(cfg.engine_opts.phc_config.enabled);
    EXPECT_EQ(cfg.engine_opts.phc_config.device, "emac0");
    EXPECT_EQ(cfg.engine_opts.phc_config.step_threshold_ns, 100'000'000);
    EXPECT_EQ(cfg.qnx.bpf_device_prefix, "/dev/bpf");
    EXPECT_TRUE(cfg.qnx.see_sent);
}

}  // namespace
}  // namespace ts
}  // namespace score
