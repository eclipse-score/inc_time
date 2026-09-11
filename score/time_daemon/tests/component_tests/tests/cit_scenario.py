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
from pathlib import Path

import pytest
from testing_utils import BazelTools, BuildTools, LogContainer, Scenario


class TimeDaemonCitScenario(Scenario):
    """Base class for time_daemon component integration tests."""

    @pytest.fixture(scope="class")
    def build_tools(self) -> BuildTools:
        return BazelTools(option_prefix="cpp")

    @pytest.fixture(scope="class")
    def logs_target(self, target_path: Path, logs: LogContainer) -> LogContainer:
        return logs.get_logs(field="target", pattern=f"{target_path.name}.*")

    @pytest.fixture(scope="class")
    def logs_info_level(self, logs_target: LogContainer) -> LogContainer:
        return logs_target.get_logs(field="level", value="INFO")

    @pytest.fixture(autouse=True)
    def print_to_report(
        self,
        request: pytest.FixtureRequest,
        logs: LogContainer,
        logs_target: LogContainer,
    ) -> None:
        traces_param = request.config.getoption("--traces")
        match traces_param:
            case "all":
                traces = logs
            case "target":
                traces = logs_target
            case "none":
                traces = LogContainer()
            case _:
                raise RuntimeError(f'Invalid "--traces" value: {traces_param}')

        for trace in traces:
            print(trace)
