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

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files", "strip_prefix")
load("@score_rules_imagefs//rules/qnx:ifs.bzl", "qnx_ifs")

pkg_files(
    name = "time_slave_bin",
    srcs = ["//score/time_slave"],
    attributes = pkg_attributes(mode = "0755"),
    prefix = "/opt/time_slave/bin",
    strip_prefix = strip_prefix.files_only(),
    target_compatible_with = ["@platforms//os:qnx"],
)

pkg_files(
    name = "time_slave_etc",
    srcs = ["time_slave_config.json"],
    attributes = pkg_attributes(mode = "0644"),
    prefix = "/opt/time_slave/etc",
    target_compatible_with = ["@platforms//os:qnx"],
)

qnx_ifs(
    name = "ifs",
    srcs = [
        ":time_slave_bin",
        ":time_slave_etc",
    ],
    build_file = "init.build",
    extra_build_files = ["tools.build"],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:qnx"],
    visibility = ["//visibility:public"],
)
