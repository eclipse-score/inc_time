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

[
    cc_library(
        name = name,
        testonly = testonly,
        srcs = [
            "factory.cpp",
        ],
        hdrs = [
            "factory.h",
            "svt_verification_machine.h",
        ],
        features = COMPILER_WARNING_FEATURES,
        tags = ["QM"],
        visibility = ["//score/time_daemon:__subpackages__"],
        deps = [
            "//score/time_daemon/src/common/data_types:ptp_time_info",
            "//score/time_daemon/src/common/machines",
            "//score/time_daemon/src/verification_machine/core:verification_machine",
            "//score/time_daemon/src/verification_machine/svt/validators",
            "@score_baselibs//score/mw/log:frontend",
        ] + deps,
    )
    for name, testonly, deps in [
        (
            "svt_verification_machine",
            False,
            [
                "//score/time/high_res_steady_time:high_res_steady_time",
            ],
        ),
        (
            "svt_verification_machine_for_utests",
            True,
            [
                "//score/time/high_res_steady_time:high_res_steady_time_mock",
            ],
        ),
    ]
]

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":svt_verification_machine_test",
        ":svt_verification_machine_pipeline_integration_test",
    ],
    test_suites_from_sub_packages = ["//score/time_daemon/src/verification_machine/svt/validators:unit_test_suite"],
    visibility = ["//score/time_daemon:__subpackages__"],
)

cc_test(
    name = "svt_verification_machine_test",
    srcs = [
        "svt_verification_machine_test.cpp",
    ],
    tags = ["unit"],
    deps = [
        ":svt_verification_machine_for_utests",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)

cc_test(
    name = "svt_verification_machine_pipeline_integration_test",
    srcs = [
        "svt_verification_machine_pipeline_integration_test.cpp",
    ],
    deps = [
        "//score/time_daemon/src/common/data_types:ptp_time_info",
        "//score/time_daemon/src/ptp_machine/stub:gptp_stub_machine",
        "//score/time_daemon/src/verification_machine/svt:svt_verification_machine",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/mw/log:console_only_backend",
    ],
)
