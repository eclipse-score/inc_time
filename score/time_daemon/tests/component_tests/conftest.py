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
import json
import os
from pathlib import Path

import pytest
from testing_utils import BazelTools

FAILED_CONFIGS = []


def pytest_addoption(parser):
    parser.addoption(
        "--traces",
        choices=["none", "target", "all"],
        default="none",
        help=(
            "Verbosity of traces in output. "
            '"none" - show no traces, '
            '"target" - show traces from test code, '
            '"all" - show all traces.'
        ),
    )
    parser.addoption(
        "--cpp-target-name",
        type=str,
        default="//score/time_daemon/tests/test_scenarios/cpp:test_scenarios",
        help="C++ test scenario executable Bazel target.",
    )
    parser.addoption(
        "--cpp-target-path",
        type=Path,
        help="Path to the pre-built C++ test scenario executable.",
    )
    parser.addoption(
        "--build-scenarios",
        action="store_true",
        help="Build test scenario executables before running tests.",
    )
    parser.addoption(
        "--build-scenarios-timeout",
        type=float,
        default=180.0,
        help="Build command timeout in seconds. Default: %(default)s",
    )
    parser.addoption(
        "--default-execution-timeout",
        type=float,
        default=10.0,
        help="Default scenario execution timeout in seconds. Default: %(default)s",
    )
    parser.addoption(
        "--bazel-config",
        type=str,
        default="time-x86_64-linux",
        help="Bazel config to use when building C++ test scenarios. Default: %(default)s",
    )


@pytest.hookimpl(tryfirst=True)
def pytest_sessionstart(session):
    try:
        if session.config.getoption("--build-scenarios"):
            build_timeout = session.config.getoption("--build-scenarios-timeout")
            print("Building C++ test scenarios executable...")
            bazel_config = session.config.getoption("--bazel-config")
            bazel_tools = BazelTools(option_prefix="cpp", build_timeout=build_timeout)
            cpp_target_name = session.config.getoption("--cpp-target-name")
            bazel_tools.build(cpp_target_name, f"--config={bazel_config}")
    except Exception as e:
        pytest.exit(str(e), returncode=1)


def pytest_html_report_title(report):
    report.title = "Time Daemon Component Integration Tests Report"


def pytest_html_results_table_header(cells):
    cells.insert(1, "<th>Test Input</th>")
    cells.insert(2, "<th>Description</th>")
    cells.insert(3, "<th>Test Scenario Name</th>")
    cells.insert(4, "<th>Test Scenario Command</th>")


def pytest_html_results_table_row(report, cells):
    cells.insert(
        1,
        f'<td><pre style="white-space:pre-wrap;word-wrap:break-word">{json.dumps(report.input)}</pre></td>',
    )
    cells.insert(2, f"<td><pre>{report.description}</pre></td>")
    cells.insert(3, f"<td><pre>{report.scenario}</pre></td>")
    cells.insert(4, f"<td><pre>{report.command}</pre></td>")


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    report = outcome.get_result()
    report.description = str(item.function.__doc__)
    report.scenario = item.funcargs.get("scenario_name", "")
    report.input = item.funcargs.get("test_config", "")

    command = []
    for token in item.funcargs.get("command", ""):
        if " " in token:
            command.append(f"'{token}'")
        else:
            command.append(token)
    report.command = " ".join(command)

    if report.failed:
        FAILED_CONFIGS.append(
            {
                "nodeid": report.nodeid,
                "command": report.command,
            }
        )


def pytest_terminal_summary(terminalreporter):
    if not FAILED_CONFIGS:
        return
    terminalreporter.write_sep("=", "Failed tests reproduction info")
    terminalreporter.write_line(
        "Run failed scenarios from the repo root working directory\n"
    )
    for entry in FAILED_CONFIGS:
        terminalreporter.write_line(
            f"{entry['nodeid']} | Run command:\n{entry['command']}\n"
        )
