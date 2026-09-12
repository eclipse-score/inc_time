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

Vehicle Time Example
====================

Overview
--------

The ``vehicle_time`` example demonstrates how to use the SCORE library's VehicleClock
in combination with HighResSteadyClock. This example shows how to work with
PTP-synchronized vehicle time alongside local monotonic time, which is essential
for automotive applications requiring distributed time synchronization.

What it does
------------

This example creates a ``VehicleTimeHandler`` wrapper class that:

- Provides access to both SCORE ``VehicleClock`` and ``HighResSteadyClock``
- Returns combined time reports with status information
- Demonstrates initialization patterns for vehicle time backends
- Shows how to monitor time synchronization quality
- Can be unit tested with independent clock mocks

The main program:

- Initializes the vehicle time backend
- Runs a loop reading both time sources simultaneously
- Displays time values, reliability, and synchronization status
- Handles SIGINT/SIGTERM for clean shutdown

Building and Running
--------------------

To build and run the example:

.. code-block:: bash

   # Build the example
   bazel build //examples/time/vehicle_time

   # Run the example
   bazel run //examples/time/vehicle_time

   # Or run the built binary directly
   ./bazel-bin/examples/time/vehicle_time/src/vehicle_time

**Note**: The vehicle time backend requires proper initialization. The example will
exit with error code 1 if initialization fails (e.g., no PTP service available).

Output Format
-------------

The program outputs lines in this format:

.. code-block:: text

   VehicleTime + HighResSteadyTime printer started. Press Ctrl+C to stop.
   [0]  vehicle=1720184400.123456789 s  hirs=12345.234567890 s  is_reliable=yes  is_consistent=yes  rate_deviation=1.23e-09
   [1]  vehicle=1720184401.234567890 s  hirs=12346.345678901 s  is_reliable=yes  is_consistent=yes  rate_deviation=1.24e-09
   ...
   Shutdown requested. Exiting.

Where:
- ``vehicle=`` shows the PTP-synchronized time in seconds.nanoseconds
- ``hirs=`` shows the local high-resolution steady time
- ``is_reliable=`` indicates if the vehicle time is synchronized and fault-free
- ``is_consistent=`` indicates if status flags are internally consistent
- ``rate_deviation=`` shows local clock deviation relative to PTP Grand Master

Code Structure
--------------

VehicleTimeHandler Class
~~~~~~~~~~~~~~~~~~~~~~~~

Located in ``examples/time/vehicle_time/src/vehicle_time_handler.h``:

.. code-block:: cpp

   class VehicleTimeHandler {
   public:
       bool Init() noexcept;
       TimeReport GetCurrentTime() const noexcept;
       void RegisterStatusCallback(VehicleTime::StatusChangedCallback callback) noexcept;
   };

   struct TimeReport {
       std::int64_t vehicle_time_ns{0};           // PTP-synchronized time
       std::int64_t high_res_steady_time_ns{0};   // Local monotonic time
       bool is_reliable{false};                   // Time sync quality
       bool is_consistent{false};                 // Status flag consistency
       double rate_deviation{0.0};                // Clock drift rate
   };

Key features:
- **Dual time sources**: Both vehicle and local time in single call
- **Status monitoring**: Reliability and consistency flags
- **Rate tracking**: Clock deviation measurement
- **Callback support**: Status change notifications delivered on the backend's worker thread

Main Program
~~~~~~~~~~~~

Located in ``examples/time/vehicle_time/src/main.cpp``:

Key features:
- Initialization error handling with early exit
- Combined time display showing both sources
- Status information formatting for monitoring
- Same signal handling pattern as other examples

Testing
-------

Run the unit tests:

.. code-block:: bash

   bazel test //examples/time/vehicle_time/src:vehicle_time_handler_test

The test shows how to mock both time sources independently:

.. code-block:: cpp

   auto vehicle_mock = std::make_shared<score::time::VehicleClockBackendMock>();
   auto hirs_mock = std::make_shared<score::time::HighResSteadyClockBackendMock>();

   score::time::test_utils::ScopedClockOverride<score::time::VehicleTime> vg{vehicle_mock};
   score::time::test_utils::ScopedClockOverride<score::time::HighResSteadyTime> hg{hirs_mock};

   EXPECT_CALL(*vehicle_mock, Init()).WillOnce(Return(true));
   EXPECT_CALL(*vehicle_mock, Now()).WillOnce(Return(...));
   EXPECT_CALL(*hirs_mock, Now()).WillOnce(Return(...));

Bazel Build Setup
-----------------

The vehicle_time example has more complex dependencies due to dual time sources and initialization.

Target Structure
~~~~~~~~~~~~~~~~

From ``examples/time/vehicle_time/src/BUILD``:

.. code-block:: python

   cc_library(
       name = "time_handler",
       hdrs = ["vehicle_time_handler.h"],
       deps = [
           "//score/time/vehicle_time:interface",
           "//score/time/high_res_steady_time:interface",
       ],
   )

   cc_binary(
       name = "vehicle_time",
       srcs = ["main.cpp"],
       deps = [
           ":time_handler",
           "//score/time/vehicle_time",           # VehicleTime production backend
           "//score/time/high_res_steady_time",   # HIRS production backend
           "@score_baselibs//score/mw/log:console_only_backend",
       ],
   )

   cc_test(
       name = "vehicle_time_handler_test",
       srcs = ["vehicle_time_handler_test.cpp"],
       tags = ["exclusive", "unit"],  # Required for ScopedClockOverride
       deps = [
           ":time_handler",
           "//score/time/vehicle_time:vehicle_time_mock",
           "//score/time/high_res_steady_time:high_res_steady_time_mock",
           "@googletest//:gtest_main",
       ],
   )

Dual Clock Dependencies
~~~~~~~~~~~~~~~~~~~~~~~

The handler depends on **two** clock interfaces:

- ``//score/time/vehicle_time:interface`` - VehicleTime tag and status types
- ``//score/time/high_res_steady_time:interface`` - HighResSteadyTime tag

The binary links **both** production backends, while tests link **both** mocks.

Key Targets
~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 50 50

   * - Target
     - Purpose
   * - ``//score/time/vehicle_time:interface``
     - VehicleTime types, status flags, callback signatures
   * - ``//score/time/vehicle_time``
     - Production backend with TimeDaemon IPC
   * - ``//score/time/vehicle_time:vehicle_time_mock``
     - Mock for Init/Now/Subscribe testing
   * - ``//score/time/high_res_steady_time:interface``
     - HighResSteadyTime tag
   * - ``//score/time/high_res_steady_time``
     - Production HIRS clock backend
   * - ``//score/time/high_res_steady_time:high_res_steady_time_mock``
     - Mock for HIRS in tests

Testing with Dual Mocks
~~~~~~~~~~~~~~~~~~~~~~~

The test demonstrates independent mock control:

.. code-block:: cpp

   auto vehicle_mock = std::make_shared<VehicleClockBackendMock>();
   auto hirs_mock = std::make_shared<HighResSteadyClockBackendMock>();

   ScopedClockOverride<VehicleTime> vg{vehicle_mock};
   ScopedClockOverride<HighResSteadyTime> hg{hirs_mock};

   EXPECT_CALL(*vehicle_mock, Init()).WillOnce(Return(true));
   EXPECT_CALL(*vehicle_mock, Now()).WillOnce(Return(vehicle_snapshot));
   EXPECT_CALL(*hirs_mock, Now()).WillOnce(Return(hirs_snapshot));

Each clock can be mocked separately with different return values and expectations.

Adapting for Your Application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When building components that use VehicleTime:

1. **Header-only dependencies** use ``:interface``:

   .. code-block:: python

      cc_library(
          name = "my_sync_component",
          hdrs = ["my_sync_component.h"],
          deps = [
              "//score/time/vehicle_time:interface",
              "//score/time/high_res_steady_time:interface",
          ],
      )

2. **Binaries** link production backends:

   .. code-block:: python

      cc_binary(
          name = "my_app",
          deps = [
              ":my_sync_component",
              "//score/time/vehicle_time",
              "//score/time/high_res_steady_time",
          ],
      )

3. **Tests** link mocks and require exclusive tag:

   .. code-block:: python

      cc_test(
          name = "my_sync_component_test",
          tags = ["exclusive", "unit"],
          deps = [
              ":my_sync_component",
              "//score/time/vehicle_time:vehicle_time_mock",
              "//score/time/high_res_steady_time:high_res_steady_time_mock",
              "@googletest//:gtest_main",
          ],
      )

The layered dependency structure keeps compile times minimal while enabling comprehensive
testing with independent clock control.
