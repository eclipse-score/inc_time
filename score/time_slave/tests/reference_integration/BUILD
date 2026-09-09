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

load("@score_itf//:defs.bzl", "py_itf_test")

py_itf_test(
    name = "time_slave_qnx_ref_integ",
    size = "enormous",
    timeout = "short",
    srcs = [
        "tests/__init__.py",
        "tests/test_time_slave_qnx.py",
    ],
    args = [
        "--qemu-config=$(rootpath qemu_config.json)",
        "--qemu-image=$(rootpath //score/time_slave/tests/reference_integration/qnx:ifs)",
    ],
    data = [
        "qemu_config.json",
        "//score/time_slave/tests/reference_integration/qnx:ifs",
    ],
    plugins = [
        "@score_itf//score/itf/plugins:qemu_plugin",
    ],
    tags = [
        "integration",
        "manual",
        "qemu",
    ],
)
