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

# Source files carrying `// # req-Id:` markers, exposed to the root
# docs() macro's scan_code attribute for source-code traceability.
filegroup(
    name = "requirement_marked_sources",
    srcs = ["vehicle_clock.cpp"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "vehicle_clock",
    srcs = ["vehicle_clock.cpp"],
    hdrs = [
        "vehicle_clock.h",
        "vehicle_clock_backend.h",
        "vehicle_time.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/time:__subpackages__"],
    deps = [
        "//score/time/clock:clock_core",
        "//score/time/ptp:ptp_types",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "vehicle_clock_mock",
    testonly = True,
    hdrs = ["vehicle_clock_backend_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":vehicle_clock",
        "//score/time/clock:clock_test_utils",
        "//score/time/vehicle_time/src/details/stub_impl",
        "@googletest//:gtest",
    ],
)

# Production entry point: vehicle_clock + production backend in a single dep.
# Production binaries dep on //score/time/vehicle_time:vehicle_time;
# test binaries dep on //score/time/vehicle_time:vehicle_time_mock.
cc_library(
    name = "vehicle_clock_impl",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],
    deps = [
        ":vehicle_clock",
        "//score/time/vehicle_time/src/details/td_impl",
    ],
)

cc_test(
    name = "vehicle_clock_test",
    srcs = ["vehicle_clock_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    tags = [
        "component",
        "exclusive",
        "unit",
    ],
    deps = [
        ":vehicle_clock_mock",
        "//score/time/clock:clock_test_utils",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "vehicle_time_status_test",
    srcs = ["vehicle_time_status_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["unit"],
    deps = [
        ":vehicle_clock",
        "//score/time/clock:clock_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":vehicle_clock_test",
        ":vehicle_time_status_test",
    ],
    test_suites_from_sub_packages = [
        "//score/time/vehicle_time/src/details:unit_test_suite",
    ],
    visibility = ["//score/time/vehicle_time:__pkg__"],
)
