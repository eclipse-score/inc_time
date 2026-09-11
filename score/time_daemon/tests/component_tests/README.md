<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# TimeDaemon Component Integration Tests (CIT)

## Scope

Component integration tests verify the TimeDaemon running as an **external
process** — its startup, runtime behaviour, IPC interfaces, and shutdown —
exercised from outside the binary.  They are NOT a replacement for unit tests or
for in-process component integration tests.

| Layer | Tool | Where it lives | What it covers |
|-------|------|----------------|----------------|
| Unit | gtest / gmock | `score/time_daemon/src/**/*_test.cpp` | Individual classes, white-box |
| Component integration (in-process) | gtest / gmock | `score/time_daemon/src/**/*_integration_test.cpp` | Multiple production objects wired together inside one process (e.g. GPTPStubMachine → SvtVerificationMachine) |
| **CIT (this layer)** | **pytest + scenario binary** | `score/time_daemon/tests/component_tests/` | **TimeDaemon running as a real binary, single- or multi-process interaction** |
| Module integration | pytest + ITF | `score/tests/module_integration_tests/` | TimeSlave ↔ TimeDaemon across components |
| System | pytest + ITF + QEMU / Docker | `score/tests/gptp_pipeline/` | Multi-binary end-to-end across OS targets |

**Rule of thumb** — if a scenario can be written by instantiating production
objects directly and linking them with gtest (i.e. everything happens inside one
test process), it belongs in the gtest layer under `src/`.  CIT is reserved for
tests that must launch the real TimeDaemon executable (or multiple binaries) and
interact with it from outside, via its real IPC/SHM interfaces.

## Architecture

```
pytest (Python)                test runner, assertions, reporting
    │
    ▼  subprocess + structured-log parsing
scenario / daemon binary       real TimeDaemon (or helper) process
    │
    ▼  IPC / SHM / command line
TimeDaemon production code     running in its own process
```

- Python side lives under `tests/` and subclasses `testing_utils.Scenario`
  (see `tests/cit_scenario.py`).
- Each pytest class declares a `scenario_name` fixture that selects which
  scenario/binary to run; results come back as exit code + structured log lines
  parsed by `testing_utils.LogContainer`.
- The `conftest.py` provides CLI options for building and running C++ scenario
  binaries (e.g. `--cpp-target-name`, `--build-scenarios`).

## Status

The Python driver scaffolding (`conftest.py`, `cit_scenario.py`, `result_code.py`,
requirements, virtualenv) is in place.  Active CIT scenarios will be added here
once the TimeDaemon binary can be launched standalone for process-level
integration testing.

In-process multi-module integration scenarios live in gtest under `src/` (e.g.
`svt_verification_machine_pipeline_integration_test.cpp`).

## Running

When scenario targets exist, run with:

```bash
bazel test //score/time_daemon/tests/component_tests:<target_name>
```
