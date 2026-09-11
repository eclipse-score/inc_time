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

# Source files carrying `// # req-Id:` markers, exposed to
# //score/time:docs_bundle via its code_targets attribute.
filegroup(
    name = "requirement_marked_sources",
    srcs = ["steady_clock.cpp"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "steady_clock",
    srcs = ["steady_clock.cpp"],
    hdrs = [
        "steady_clock.h",
        "steady_clock_backend.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/time:__subpackages__"],
    deps = [
        "//score/time/clock:clock_core",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "steady_clock_mock",
    testonly = True,
    hdrs = ["steady_clock_backend_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":steady_clock",
        "//score/time/clock:clock_test_utils",
        "//score/time/steady_time/src/details/stub_impl",
        "@googletest//:gtest",
    ],
)

# Production entry point: steady_clock + production backend in a single dep.
cc_library(
    name = "steady_clock_impl",
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":steady_clock",
        "//score/time/steady_time/src/details/steady_time_impl",
    ],
)

cc_test(
    name = "steady_clock_adapter_test",
    srcs = ["steady_clock_adapter_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    tags = [
        "component",
        "exclusive",
        "unit",
    ],
    deps = [
        ":steady_clock_mock",
        "//score/time/clock:clock_test_utils",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "steady_clock_integration_test",
    srcs = ["steady_clock_integration_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["unit"],
    deps = [
        ":steady_clock_impl",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":steady_clock_adapter_test",
        ":steady_clock_integration_test",
    ],
    test_suites_from_sub_packages = [
        "//score/time/steady_time/src/details:unit_test_suite",
    ],
    visibility = ["//score/time/steady_time:__pkg__"],
)
