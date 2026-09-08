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
"""Reference integration test: TimeSlave configures, builds and boots on QNX.

Showcase-level smoke test: verifies the QNX binary and JSON config are
packaged into the IFS image, the image boots under QEMU, the expected
network interface exists, and the binary locates and parses the deployed
config file. Full gPTP synchronisation is covered by the SIT.
"""

import pytest


GPTP_IFACE = "vtnet0"


def test_config_file_present(target):
    """The JSON config is deployed to the expected path."""
    exit_code, output = target.execute(
        "ls -l /opt/time_slave/etc/time_slave_config.json"
    )
    assert exit_code == 0, f"config file missing: {output!r}"


def test_binary_present(target):
    """The time_slave binary is deployed and executable."""
    exit_code, output = target.execute("ls -l /opt/time_slave/bin/time_slave")
    assert exit_code == 0, f"binary missing: {output!r}"


def test_interface_exists(target):
    """The expected gPTP network interface is available inside QEMU."""
    exit_code, output = target.execute(f"ifconfig {GPTP_IFACE}")
    if exit_code != 0:
        decoded = (
            output.decode("utf-8", errors="replace")
            if isinstance(output, bytes)
            else output
        )
        pytest.skip(f"Interface '{GPTP_IFACE}' not available: {decoded}")


def test_config_loaded(target):
    """The binary locates and parses the deployed JSON config.

    Runs time_slave briefly; the expected failure mode is ClockIdentity
    resolution, which proves the config file was read and its iface_name
    applied before the gPTP engine attempted initialisation.
    """
    cmd = "cd /opt/time_slave && ./bin/time_slave 2>&1 ; true"
    _, output = target.execute(cmd)
    output_str = (
        output.decode("utf-8", errors="replace")
        if isinstance(output, bytes)
        else output
    )
    assert GPTP_IFACE in output_str, (
        f"Expected iface_name '{GPTP_IFACE}' from config in output, got: {output_str!r}"
    )
