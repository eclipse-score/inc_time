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

load("@score_docs_as_code//:docs.bzl", "docs")
load("@score_tooling//:defs.bzl", "copyright_checker", "dash_license_checker", "setup_starpls")
load("@score_tooling//third_party/format:macros.bzl", "use_format_targets")
load("//:project_config.bzl", "PROJECT_CONFIG")

setup_starpls(
    name = "starpls_server",
    visibility = ["//visibility:public"],
)

docs(
    bundles = [
        {
            "bundle": "//score/time_slave:docs_bundle",
            "mount_at": "time_slave",
            "attach_to": "module/index",
        },
        {
            "bundle": "//score/time_daemon:docs_bundle",
            "mount_at": "time_daemon",
            "attach_to": "module/index",
        },
        {
            "bundle": "//score/ts_client:docs_bundle",
            "mount_at": "ts_client",
            "attach_to": "module/index",
        },
        {
            "bundle": "//score/time:docs_bundle",
            "mount_at": "time",
            "attach_to": "module/index",
        },
    ],
    external_needs = [
        "@score_process_description//:needs_json_file",
        "@score_platform//:needs_json_file",
    ],
    project = "S-CORE Time",
    project_url = "https://eclipse-score.github.io/time",
    source_dir = "docs",
)

copyright_checker(
    name = "copyright",
    srcs = [
        ".devcontainer",
        ".github",
        "BUILD",
        "MODULE.bazel",
        "docs",
        "examples",
        "score",
        "tools",
    ],
    config = "@score_tooling//cr_checker/resources:config",
    template = "@score_tooling//cr_checker/resources:templates",
    visibility = ["//visibility:public"],
)

dash_license_checker(
    src = "//examples:cargo_lock",
    file_type = "",  # let it auto-detect based on project_config
    project_config = PROJECT_CONFIG,
    visibility = ["//visibility:public"],
)

exports_files(
    [
        # Used by the @score_tooling coverage reporter to locate the workspace root.
        "MODULE.bazel",
    ],
)

# Add targets for formatting checks
use_format_targets(languages = [
    "python",
    "rust",
    "starlark",
    "yaml",
    "cpp",
])

# Aggregated component-test suite. Component tests exercise a clock facade
# (Clock<T>) together with a mocked backend, i.e. more than one unit of code
# but without a real driver. Run with:
#   bazel test --config=time-x86_64-linux //:component_tests
test_suite(
    name = "component_tests",
    tests = [
        "//score/time/high_res_steady_time/src:high_res_steady_clock_test",
        "//score/time/steady_time/src:steady_clock_test",
        "//score/time/system_time/src:system_clock_test",
        "//score/time/vehicle_time/src:vehicle_clock_test",
    ],
    visibility = ["//visibility:public"],
)

# Unit tests: every cc_test under //score/... is already tagged "unit",
# so `bazel test //score/...` is the canonical unit-tests invocation.
# No aggregate test_suite is needed here — a `test_suite` in a top-level
# BUILD file cannot use `//score/...` as an element of its `tests`
# attribute (package wildcards are rejected).
