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
load("@score_docs_as_code//:docs.bzl", "docs_bundle")

docs_bundle(
    name = "docs_bundle",
    code_targets = [
        "//score/time/clock/src:requirement_marked_sources",
        "//score/time/high_res_steady_time/src:requirement_marked_sources",
        "//score/time/steady_time/src:requirement_marked_sources",
        "//score/time/system_time/src:requirement_marked_sources",
        "//score/time/vehicle_time/src:requirement_marked_sources",
    ],
    source_dir = "docs",
    visibility = ["//visibility:public"],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [],
    test_suites_from_sub_packages = [
        "//score/time/clock:unit_test_suite",
        "//score/time/ptp:unit_test_suite",
        "//score/time/high_res_steady_time:unit_test_suite",
        "//score/time/vehicle_time:unit_test_suite",
        "//score/time/steady_time:unit_test_suite",
        "//score/time/system_time:unit_test_suite",
    ],
    visibility = ["//score:__pkg__"],
)
