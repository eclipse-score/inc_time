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

# Production backend: reads live PTP data from the TimeDaemon via SvtReceiver and
# delivers subscription callbacks from a dedicated worker thread.
# Link this target (or the forwarding alias //score/time/vehicle_time/src/details:td_impl)
# into the production binary to provide CreateBackend<VehicleTime>().
cc_library(
    name = "td_impl",
    srcs = [
        "svt_callback_dispatcher.cpp",
        "vehicle_clock_backend_impl.cpp",
        "//score/time/vehicle_time/src/details:logging_contexts",  # internal header
    ],
    hdrs = [
        "svt_callback_wrapper.h",
        "svt_callback_dispatcher.h",
        "vehicle_clock_backend_impl.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/time/vehicle_time:__subpackages__"],
    deps = [
        "//score/time/high_res_steady_time/src:high_res_steady_clock",
        "//score/time/vehicle_time/src:vehicle_clock",
        "//score/time_daemon/src/ipc:svt_receiver",
        "@score_baselibs//score/concurrency:condition_variable",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log:frontend",
    ],
)

cc_test(
    name = "svt_callback_wrapper_test",
    srcs = [
        "svt_callback_wrapper.h",
        "svt_callback_wrapper_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["unit"],
    deps = [
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_test(
    name = "svt_callback_dispatcher_test",
    srcs = [
        "svt_callback_dispatcher.cpp",
        "svt_callback_dispatcher.h",
        "svt_callback_wrapper.h",
        "svt_callback_dispatcher_test.cpp",
        "svt_test_helpers.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = [
        "exclusive",
        "unit",
    ],
    deps = [
        "//score/time/vehicle_time/src:vehicle_clock",
        "//score/time_daemon/src/ipc:svt_receiver_mock",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/concurrency:condition_variable",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/coverage_termination_handler",
    ],
)

cc_test(
    name = "vehicle_clock_backend_impl_test",
    srcs = [
        "svt_callback_wrapper.h",
        "svt_callback_dispatcher.cpp",
        "svt_callback_dispatcher.h",
        "svt_test_helpers.h",
        "vehicle_clock_backend_impl.cpp",
        "vehicle_clock_backend_impl.h",
        "vehicle_clock_backend_impl_test.cpp",
        "//score/time/vehicle_time/src/details:logging_contexts",  # internal header
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = [
        "exclusive",
        "unit",
    ],
    deps = [
        "//score/time/high_res_steady_time:high_res_steady_time_mock",
        "//score/time/vehicle_time/src:vehicle_clock",
        "//score/time_daemon/src/ipc:svt_receiver_mock",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/concurrency:condition_variable",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/coverage_termination_handler",
        "@score_baselibs//score/mw/log:console_only_backend",
        "@score_baselibs//score/mw/log:frontend",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":svt_callback_dispatcher_test",
        ":svt_callback_wrapper_test",
        ":vehicle_clock_backend_impl_test",
    ],
    visibility = ["//score/time/vehicle_time:__subpackages__"],
)
