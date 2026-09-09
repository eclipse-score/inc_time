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

cc_binary(
    name = "time_slave",
    srcs = [
        "main.cpp",
        "time_slave.cpp",
        "time_slave.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score/time_slave:__pkg__"],
    deps = [
        "//score/time_slave/src/application/configuration",
        "//score/time_slave/src/common:logging_contexts",
        "//score/time_slave/src/gptp:gptp_engine",
        "//score/ts_client/src:gptp_ipc",
        "@score_baselibs//score/mw/log:console_only_backend",
        "@score_lifecycle//score/launch_manager:lifecycle_cc",
    ],
)
