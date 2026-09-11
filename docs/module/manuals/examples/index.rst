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

Examples User Manual
====================

This manual explains the examples included in the ``examples/time/`` subdirectory and how they
can be used as patterns for building applications with the SCORE time library.

Overview
--------

The ``examples/time/`` directory contains four working examples that demonstrate how to use
different SCORE time sources:

- **Basic Clock Examples** (system_time, steady_time, high_res_steady_time): Simple periodic time printers showing the common pattern for reading time from SCORE clocks
- **Vehicle Time Example** (vehicle_time): More complex example showing PTP-synchronized time with initialization, status monitoring, and dual time sources

All examples follow consistent patterns and can be used as starting points for real applications.

Building and Running Examples
------------------------------

All examples use Bazel:

.. code-block:: bash

   # Build all examples
   bazel build //examples/...

   # Run specific example
   bazel run //examples/time/system_time

   # Run tests
   bazel test //examples/time/system_time/src:system_time_handler_test

Common Patterns
---------------

All examples share these implementation patterns:

- **Handler wrapper classes** providing clean APIs over SCORE Clock<Tag> types
- **TimeReport structs** containing time data with consistent field naming
- **Signal handling** for graceful shutdown on SIGINT/SIGTERM
- **Unit test patterns** using ScopedClockOverride for dependency injection
- **Nanosecond precision** throughout all time calculations

.. toctree::
   :maxdepth: 2
   :caption: Examples:

   basic_clocks
   vehicle_time
   testing
