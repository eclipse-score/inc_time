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
    srcs = ["clock.h"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "clock_core",
    hdrs = [
        "availability_hook.h",
        "clock.h",
        "clock_snapshot.h",
        "clock_status.h",
        "clock_traits.h",
        "initialization_hook.h",
        "no_status.h",
        "subscription_hook.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/time:__subpackages__"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

# Test-only clock utilities: RAII override guard + direct-backend factory.
# Mock libraries and test targets depend on this; production code must not.
cc_library(
    name = "clock_test_utils",
    testonly = True,
    hdrs = [
        "clock_test_factory.h",
        "scoped_clock_override.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/time:__subpackages__"],
    deps = [":clock_core"],
)

cc_test(
    name = "clock_status_test",
    srcs = ["clock_status_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["unit"],
    deps = [
        ":clock_core",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [":clock_status_test"],
    visibility = ["//score/time/clock:__pkg__"],
)
