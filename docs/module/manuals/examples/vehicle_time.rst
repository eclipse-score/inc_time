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
- **Callback support**: Status change notifications (future feature)

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

Use Cases
---------

VT1 — Time Polling with Status Check
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Obtain a snapshot and inspect the synchronization quality before using the time
value. ``Now()`` returns a single immutable ``ClockSnapshot`` — the timepoint and its
``VehicleTimeStatus`` are always fetched together, with no separate status call needed.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/vehicle_time/vt1_polling.puml
   :alt: VT1 — Time polling with status check

.. raw:: html

   </div>

.. code-block:: cpp

   #include "score/time/vehicle_time/src/vehicle_clock.h"

   void MyComponent::CheckTime()
   {
       auto clock = score::time::VehicleClock::GetInstance();
       auto snapshot = clock.Now();

       if (snapshot.Status().IsReliable()) {
           auto tp = snapshot.TimePoint();
           // use tp ...
       } else if (snapshot.Status().IsFlagActive(
                      score::time::VehicleTime::StatusFlag::kTimeOut)) {
           HandleTimeout();
       }
   }

.. note::

   ``Init()`` must be called once during application startup before ``Now()`` is expected
   to return synchronized data (see VT2).  Without it, ``Now()`` returns a snapshot with
   no flags set (``IsConsistent()`` returns ``false``).

**Status flags:**

+---------------------------+------------------------------------------------------------+
| Flag                      | Meaning                                                    |
+===========================+============================================================+
| ``kSynchronized``         | Synchronized at least once to the PTP Grand Master         |
+---------------------------+------------------------------------------------------------+
| ``kTimeOut``              | No sync message received within the configured time window |
+---------------------------+------------------------------------------------------------+
| ``kTimeLeapFuture``       | A large forward adjustment was applied                     |
+---------------------------+------------------------------------------------------------+
| ``kTimeLeapPast``         | A large backward adjustment was applied                    |
+---------------------------+------------------------------------------------------------+

``VehicleTimeStatus::IsReliable()`` returns ``true`` only when ``kSynchronized`` is set
**and** none of ``{kTimeOut, kTimeLeapFuture, kTimeLeapPast}`` is set.
``VehicleTimeStatus::HasBeenSynchronized()`` returns ``true`` whenever ``kSynchronized``
has been set at least once during this lifecycle, regardless of current fault flags.
``VehicleTimeStatus::IsConsistent()`` checks that the flag combination is internally
valid (at least one flag set, and not both leap flags simultaneously).

These three methods belong to ``VehicleTimeStatus`` and encode VehicleTime-domain
semantics.  ``ClockStatus<FlagEnumT>`` itself exposes only generic bit-manipulation
(``IsFlagActive``, ``IsAnyOfFlagsActive``, ``AddFlag``) and the domain-specific
``PrintTo()`` specialization.

The full working implementation of this pattern is in
``examples/time/vehicle_time/src/vehicle_time_handler.h`` (``GetCurrentTime()``) and
``examples/time/vehicle_time/src/main.cpp``.

VT2 — Initialization and Readiness Check
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``VehicleTime`` requires an explicit ``Init()`` call to open the IPC channel to the
time daemon before any time data becomes available.  Until ``Init()`` returns ``true``,
``Now()`` returns a snapshot with no flags set (``IsConsistent()`` returns ``false``) and
``IsAvailable()`` returns ``false``.

After a successful ``Init()``, ``IsAvailable()`` returns ``true`` immediately.  The
non-blocking ``IsAvailable()`` probe and the blocking ``WaitUntilAvailable()`` are useful
when ``Init()`` is retried on a background thread.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/vehicle_time/vt2_availability.puml
   :alt: VT2 — Initialization and readiness check

.. raw:: html

   </div>

**Simple startup (same thread):**

.. code-block:: cpp

   #include "score/time/vehicle_time/src/vehicle_clock.h"

   bool MyService::Startup()
   {
       auto clock = score::time::VehicleClock::GetInstance();
       if (!clock.Init()) {
           LOG_ERROR("VehicleTime: failed to open IPC channel");
           return false;
       }
       auto snapshot = clock.Now();
       // ...
       return true;
   }

**Blocking wait when Init is retried from a background thread:**

.. code-block:: cpp

   #include "score/time/vehicle_time/src/vehicle_clock.h"
   #include <score/stop_token.hpp>
   #include <chrono>

   void MyService::WaitForClock(const score::cpp::stop_token& stop)
   {
       auto clock = score::time::VehicleClock::GetInstance();
       const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
       if (!clock.WaitUntilAvailable(stop, deadline)) {
           LOG_ERROR("VehicleTime did not become available within 30 s");
           return;
       }
       auto snapshot = clock.Now();
       // ...
   }

.. note::

   ``Init()``, ``IsAvailable()``, and ``WaitUntilAvailable()`` are **only available on
   clock domains that require explicit initialisation** (currently ``VehicleTime``).
   Calling them on ``HighResSteadyTime``, ``SteadyClock``, or ``SystemClock`` is a **compile
   error** — those clocks are always ready.

The full working implementation of this pattern is in
``examples/time/vehicle_time/src/vehicle_time_handler.h`` (``Init()``) and
``examples/time/vehicle_time/src/main.cpp``.

VT3 — Async PTP Protocol Data Subscription
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``VehicleTime`` exposes two PTP protocol data callbacks, intended primarily for
diagnostics and PTP data sanity checks:

- ``TimeSlaveSyncData<VehicleTime>`` — fired on each PTP Sync/Follow_Up message pair;
  carries the offset, rate correction, and raw timestamps computed by the TimeSlave.
- ``PDelayMeasurementData<VehicleTime>`` — fired when a peer-delay measurement cycle
  completes; carries the measured peer delay and associated timestamps.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/vehicle_time/vt3_subscription.puml
   :alt: VT3 — Async PTP protocol data subscription

.. raw:: html

   </div>

.. warning::

   Both PTP data callbacks (``TimeSlaveSyncData`` and ``PDelayMeasurementData``) are
   **not yet delivered**.  Calling ``Subscribe<...>()`` compiles and runs without error,
   but the registered callbacks will never be invoked.  Delivery will be wired from a
   dedicated background thread in a future change.

.. code-block:: cpp

   #include "score/time/vehicle_time/src/vehicle_clock.h"
   #include "score/time/ptp/src/time_slave_sync_data.h"
   #include "score/time/ptp/src/pdelay_measurement_data.h"

   void MyDiagHandler::RegisterCallbacks()
   {
       auto clock = score::time::VehicleClock::GetInstance();

       clock.Subscribe<score::time::TimeSlaveSyncData<score::time::VehicleTime>>(
           [this](const auto& data) { OnTimeSyncData(data); });

       clock.Subscribe<score::time::PDelayMeasurementData<score::time::VehicleTime>>(
           [this](const auto& data) { OnPDelayData(data); });
   }

   void MyDiagHandler::Shutdown()
   {
       auto clock = score::time::VehicleClock::GetInstance();
       clock.Unsubscribe<score::time::TimeSlaveSyncData<score::time::VehicleTime>>();
       clock.Unsubscribe<score::time::PDelayMeasurementData<score::time::VehicleTime>>();
   }

.. warning::

   Callbacks are invoked on the **backend thread** — the callback implementation must be
   thread-safe.

VT4 — Synchronization Status Subscription
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Subscribe to ``VehicleTimeStatus`` changes to react when the clock synchronization state
changes — for example, when the timebase becomes synchronized and is ready to use, when a
timeout occurs, or when a large time leap is applied.  This is the primary mechanism for
application components to know that ``VehicleTime`` is reliable and may be safely read.

Unlike the PTP protocol data callbacks in VT3, ``VehicleTimeStatus`` carries no protocol
internals.  It delivers the same status value already available via ``Now().Status()``,
but pushed proactively on every change rather than polled per call.

The callback fires unconditionally on the first PTP status update received after
registration, and subsequently only when the flag set changes.  Rate deviation is
excluded from the comparison.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/vehicle_time/vt4_status_subscription.puml
   :alt: VT4 — Synchronization status subscription

.. raw:: html

   </div>

.. warning::

   The ``VehicleTimeStatus`` callback is **not yet delivered**.  Calling
   ``Subscribe<VehicleTimeStatus>()`` compiles and runs without error, but the registered
   callback will never be invoked.  Delivery will be wired from a dedicated background
   thread in a future change.

.. code-block:: cpp

   #include "score/time/vehicle_time/src/vehicle_clock.h"

   void MyService::WatchClockReadiness()
   {
       auto clock = score::time::VehicleClock::GetInstance();

       clock.Subscribe<score::time::VehicleTimeStatus>(
           [this](const score::time::VehicleTimeStatus& status) {
               if (status.IsReliable()) {
                   OnClockReady();
               } else if (status.HasBeenSynchronized()) {
                   OnClockDegraded();
               } else {
                   OnClockUnavailable();
               }
           });
   }

   void MyService::Shutdown()
   {
       auto clock = score::time::VehicleClock::GetInstance();
       clock.Unsubscribe<score::time::VehicleTimeStatus>();
   }

.. warning::

   Callbacks are invoked on the **backend thread** — the callback implementation must be
   thread-safe.

The full working implementation of this pattern is in
``examples/time/vehicle_time/src/vehicle_time_handler.h``
(``RegisterStatusCallback()`` / ``UnregisterStatusCallback()``).

VT5 — Status Flag Inspection
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When mapping ``VehicleTime`` status to diagnostic outputs such as DTC bitmasks, use
``IsFlagActive(flag)`` with the ``VehicleTime::StatusFlag`` enum to access individual
bits.  For the higher-level reliability predicates (``IsReliable()``,
``HasBeenSynchronized()``), see the status flag table and method descriptions in VT1.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/vehicle_time/vt5_diagnostics.puml
   :alt: VT5 — Status flag inspection

.. raw:: html

   </div>

.. code-block:: cpp

   #include "score/time/vehicle_time/src/vehicle_clock.h"
   #include <map>

   using SvtFlag = score::time::VehicleTime::StatusFlag;

   static const std::map<SvtFlag, uint8_t> kDiagBitMap = {
       {SvtFlag::kSynchronized,   0x01U},
       {SvtFlag::kTimeOut,        0x02U},
       {SvtFlag::kTimeLeapFuture, 0x04U},
       {SvtFlag::kTimeLeapPast,   0x08U},
       {SvtFlag::kUnknown,        0x80U},
   };

   uint8_t BuildDiagByte(const score::time::VehicleTimeStatus& status)
   {
       uint8_t result{0U};
       for (const auto& entry : kDiagBitMap) {
           if (status.IsFlagActive(entry.first)) {
               result |= entry.second;
           }
       }
       return result;
   }

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
