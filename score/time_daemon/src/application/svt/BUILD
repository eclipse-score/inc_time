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
            "svt_handler.cpp",
        ],
        hdrs = [
            "factory.h",
            "svt_handler.h",
        ],
        features = COMPILER_WARNING_FEATURES,
        visibility = ["//score/time_daemon/src/application:__pkg__"],
        deps = [
            "//score/time_daemon/src/application:timebase_handler",
            "//score/time_daemon/src/application/job_runner",
            "//score/time_daemon/src/control_flow_divider/ptp:ptp_control_flow_divider",
            "//score/time_daemon/src/ipc/svt/publisher:factory",
            "//score/time_daemon/src/msg_broker",
        ] + deps,
    )
    for name, testonly, deps in [
        (
            "svt_handler",
            False,
            [
                "//score/time_daemon/src/ptp_machine:shm_ptp_machine",
                "//score/time_daemon/src/verification_machine/svt:svt_verification_machine",
            ],
        ),
        (
            "svt_handler_for_utests",
            True,
            [
                "//score/time_daemon/src/ptp_machine:shm_ptp_machine",
                "//score/time_daemon/src/verification_machine/svt:svt_verification_machine_for_utests",
            ],
        ),
    ]
]

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        # ":svt_handler_integration_test",
    ],
    visibility = ["//score/time_daemon/src/application:__pkg__"],
)

# TODO: fixme
# cc_test(
#     name = "svt_handler_integration_test",
#     srcs = [
#         "svt_handler_integration_test.cpp",
#     ],
#     tags = [
#         "manual",
#         "unit",
#     ],
#     deps = [
#         ":svt_handler_for_utests",
#         "//score/time_daemon/src/ipc:svt_receiver",
#         "@googletest//:gtest",
#         "@googletest//:gtest_main",
#     ],
# )
