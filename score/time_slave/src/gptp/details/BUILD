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

load("@score_baselibs//:bazel/unit_tests.bzl", "cc_unit_test_suites_for_host_and_qnx")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "ptp_types",
    hdrs = ["ptp_types.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
)

cc_library(
    name = "raw_socket",
    hdrs = ["raw_socket.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [],
)

cc_library(
    name = "os_syscalls",
    hdrs = ["os_syscalls.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [],
)

cc_library(
    name = "network_identity",
    hdrs = ["network_identity.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [":ptp_types"],
)

cc_library(
    name = "raw_socket_impl",
    srcs = select({
        "@platforms//os:qnx": ["//score/time_slave/src/gptp/platform/qnx:raw_socket_src"],
        "//conditions:default": ["//score/time_slave/src/gptp/platform/linux:raw_socket_src"],
    }),
    hdrs = ["raw_socket_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    linkopts = select({
        "@platforms//os:qnx": ["-lsocket"],
        "//conditions:default": [],
    }),
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [
        ":os_syscalls",
        ":ptp_types",
        ":raw_socket",
        "//score/time_slave/src/common:logging_contexts",
        "@score_baselibs//score/mw/log:frontend",
    ],
)

cc_library(
    name = "frame_codec",
    srcs = ["frame_codec.cpp"],
    hdrs = ["frame_codec.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [":ptp_types"],
)

cc_library(
    name = "message_parser",
    srcs = ["message_parser.cpp"],
    hdrs = ["message_parser.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [":ptp_types"],
)

cc_library(
    name = "clock_util",
    hdrs = ["clock_util.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [":ptp_types"],
)

cc_library(
    name = "sync_state_machine",
    srcs = ["sync_state_machine.cpp"],
    hdrs = ["sync_state_machine.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [
        ":clock_util",
        ":ptp_types",
        "//score/ts_client/src:gptp_ipc",
    ],
)

cc_library(
    name = "pdelay_measurer",
    srcs = ["pdelay_measurer.cpp"],
    hdrs = ["pdelay_measurer.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [
        ":frame_codec",
        ":ptp_types",
        ":raw_socket",
        "//score/ts_client/src:gptp_ipc",
    ],
)

cc_library(
    name = "network_identity_impl",
    srcs = select({
        "@platforms//os:qnx": ["//score/time_slave/src/gptp/platform/qnx:network_identity_src"],
        "//conditions:default": ["//score/time_slave/src/gptp/platform/linux:network_identity_src"],
    }),
    hdrs = ["network_identity_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [
        ":network_identity",
        ":ptp_types",
    ],
)

cc_library(
    name = "gptp_details",
    features = COMPILER_WARNING_FEATURES,
    tags = ["QM"],
    visibility = ["//score:__subpackages__"],
    deps = [
        ":frame_codec",
        ":message_parser",
        ":network_identity",
        ":network_identity_impl",
        ":pdelay_measurer",
        ":ptp_types",
        ":raw_socket",
        ":raw_socket_impl",
        ":sync_state_machine",
    ],
)

cc_test(
    name = "raw_socket_test",
    srcs = ["raw_socket_test.cpp"],
    tags = ["unit"],
    target_compatible_with = ["@platforms//os:linux"],
    deps = [
        ":network_identity_impl",
        ":raw_socket_impl",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)

cc_test(
    name = "pdelay_measurer_test",
    srcs = ["pdelay_measurer_test.cpp"],
    tags = ["unit"],
    deps = [
        ":pdelay_measurer",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)

cc_test(
    name = "frame_codec_test",
    srcs = ["frame_codec_test.cpp"],
    tags = ["unit"],
    deps = [
        ":frame_codec",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)

cc_test(
    name = "message_parser_test",
    srcs = ["message_parser_test.cpp"],
    tags = ["unit"],
    deps = [
        ":message_parser",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)

cc_test(
    name = "sync_state_machine_test",
    srcs = ["sync_state_machine_test.cpp"],
    tags = ["unit"],
    deps = [
        ":sync_state_machine",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":frame_codec_test",
        ":message_parser_test",
        ":pdelay_measurer_test",
        ":raw_socket_test",
        ":sync_state_machine_test",
    ],
    test_suites_from_sub_packages = [],
    visibility = ["//score:__subpackages__"],
)
