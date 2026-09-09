# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//tools/lint:json_schema_validator.bzl", "validate_json_schema_test")

filegroup(
    name = "time_slave_config_schema",
    srcs = ["time_slave_config_schema.json"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "configuration",
    srcs = ["config_parser.cpp"],
    hdrs = [
        "config_parser.h",
        "time_slave_config.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score/time_slave:__subpackages__"],
    deps = [
        "//score/time_slave/src/common:logging_contexts",
        "//score/time_slave/src/gptp:gptp_engine",
        "@score_baselibs//score/json",
        "@score_baselibs//score/json:json_parser",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)

cc_test(
    name = "config_parser_test",
    srcs = ["config_parser_test.cpp"],
    tags = ["unit"],
    target_compatible_with = ["@platforms//os:linux"],
    deps = [
        ":configuration",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)

validate_json_schema_test(
    name = "validate_time_slave_config_schema",
    json = "example/time_slave_config.json",
    schema = ":time_slave_config_schema",
    tags = ["lint"],
    target_compatible_with = ["@platforms//os:linux"],
)
