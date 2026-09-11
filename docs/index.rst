..
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

time_daemon Documentation
=========================

This documentation describes the **time_daemon** and the **score::time** module.

.. contents:: Table of Contents
   :depth: 2
   :local:

Overview
--------

**time_daemon** is a non-AUTOSAR adaptive process designed to provide synchronized vehicle time to client applications.
It supports multiple time bases including **in-vehicle synchronized time** (PTP - Precision Time Protocol) and
**external synchronized time** (absolute time base). The daemon retrieves time information from the respective time sources,
verifies and validates the timepoints, and distributes this time information across multiple clients through efficient IPC mechanisms.

The main responsibilities of time_daemon include:

- **Providing current Vehicle time** to different applications
- **Setting synchronization qualifiers** (e.g., Synchronized, Timeout, etc.)
- **Providing diagnostic information** for system monitoring
- **Supporting additional verification mechanisms** such as QualifiedVehicleTime (QVT) for safety-critical applications

For a detailed concept and architectural design, please refer to the :doc:`time_daemon documentation <time_daemon/index>`.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   features/index
   module/index


.. toctree::
   :maxdepth: 1
   :caption: Release Planning:

   release/roadmap

Project Layout
--------------

This module follows the Eclipse SCORE component structure:

- `score/time_daemon/src/`: Time daemon process sources
- `score/time_slave/src/`: Time slave process sources
- `score/time/`: Client-facing time base libraries
- `score/ts_client/`: Time synchronization client library
- `examples/`: Usage examples
- `docs/features/`: Feature-level documentation
- `.github/workflows/`: CI/CD pipelines

Quick Start
-----------

To build the module:

.. code-block:: bash

   bazel build //score/...

To run tests:

.. code-block:: bash

   bazel test //score/...

To build the documentation:

.. code-block:: bash

   bazel build //:docs

**Dependency lock file**

After modifying ``MODULE.bazel`` (adding or bumping a dependency), update the lock file:

.. code-block:: bash

   bazel mod tidy

Commit both ``MODULE.bazel`` and ``MODULE.bazel.lock`` together. The
``Process / Bzlmod Lock Check`` CI job enforces this — see
`eclipse-score/score#2628 <https://github.com/eclipse-score/score/issues/2628>`_.

**Formatting**

Check formatting for all files (Python, Starlark, YAML, C++):

.. code-block:: bash

   bazel test //:format.check

Auto-fix formatting for all files:

.. code-block:: bash

   bazel run //:format.fix

**Static Code Analysis**

Run clang-tidy (powered by ``score_cpp_policies``):

.. code-block:: bash

   bazel test --config=clang-tidy //score/...

**Sanitizers**

Run address, undefined-behaviour and leak sanitizers (powered by ``score_cpp_policies``):

.. code-block:: bash

   bazel test --config=asan_ubsan_lsan --config=x86_64-linux //score/...

Individual sanitizer aliases are also available: ``--config=asan``, ``--config=ubsan``, ``--config=lsan``.

Configuration
-------------

The `project_config.bzl` file defines metadata used by Bazel macros.

Example:

.. code-block:: python

   PROJECT_CONFIG = {
      "asil_level": "QM",
      "source_code": ["cpp", "rust"]
   }

This enables conditional behavior (e.g., choosing `clang-tidy` for C++ or `clippy` for Rust).
