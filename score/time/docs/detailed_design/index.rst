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

.. _time_detailed_design:

Detailed Design
###############

.. document:: score::time Detailed Design
   :id: doc__time_detailed_design
   :status: draft
   :version: 1
   :safety: ASIL_B
   :security: NO
   :realizes: wp__sw_implementation
   :tags: time, detailed_design

Description
-----------

``score::time`` provides a **unified, clock-domain-agnostic API** for reading time
snapshots, checking clock readiness, and subscribing to clock synchronization events —
all through a single template wrapper ``Clock<Tag>``.

The design separates two concerns:

1. **What kind of time** — expressed as a *tag struct* (``VehicleTime``,
   ``HighResSteadyTime``, ``std::chrono::steady_clock``,
   ``std::chrono::system_clock``).
2. **How to access it** — always via ``Clock<Tag>::GetInstance()``; clock-domain
   selection is a compile-time decision, enforced by the type system.

Clock Domains
~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Clock alias
     - Tag
     - Status concept
   * - ``VehicleClock``
     - ``VehicleTime``
     - ``VehicleTimeStatus``
   * - ``HighResSteadyClock``
     - ``HighResSteadyTime``
     - ``NoStatus``
   * - ``SteadyClock``
     - ``std::chrono::steady_clock``
     - ``NoStatus``
   * - ``SystemClock``
     - ``std::chrono::system_clock``
     - ``NoStatus``

**VehicleTime** is a PTP-synchronized timebase driven by the network Grand Master clock.
Each ``Now()`` call returns a ``ClockSnapshot`` that bundles the timepoint with a
``VehicleTimeStatus`` — a set of quality flags (``kSynchronized``, ``kTimeOut``,
``kTimeLeapFuture``, ``kTimeLeapPast``) and a rate-deviation measurement.

**HighResSteadyTime** is a monotonic, nanosecond-resolution clock optimized for
low-overhead timing. On QNX the backend reads the hardware cycle counter directly via
``ClockCycles()`` — no kernel call, no scheduler interaction. On Linux it delegates to
``std::chrono::high_resolution_clock``.

**SteadyClock** wraps ``std::chrono::steady_clock`` (POSIX ``CLOCK_MONOTONIC``).
Monotonic and never goes backward, making it the standard choice for measuring elapsed
time and computing timeouts.

**SystemClock** wraps ``std::chrono::system_clock`` (POSIX ``CLOCK_REALTIME``).
Represents wall-clock (UTC-based) time and may be adjusted or jump forward or backward.
Use when a calendar timestamp is needed — not for measuring elapsed time or computing timeouts.

Architecture
------------

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/architecture_layers.puml
   :alt: Architecture layer diagram

.. raw:: html

   </div>

The library has three layers:

- **Public headers** under ``score/time/<domain>/`` — tag structs and callback types that
  clients include directly.
- **Framework layer** under ``score/time/clock/`` — the ``Clock<Tag>`` wrapper, traits,
  subscription hooks, and the test utilities (``clock_test_utils`` Bazel target). This
  layer has no backend dependency.
  The ``clock_test_utils`` target (``scoped_clock_override.h``, ``clock_test_factory.h``)
  is ``testonly`` and must not appear in production deps.
- **Internal** under ``score/time/<domain>/details/`` — pure-virtual backend interfaces and
  production implementations. *Clients must never include anything from a* ``details/``
  *subfolder.*

Static View
~~~~~~~~~~~

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/class_overview.puml
   :alt: Class overview
   :width: 100%
   :align: center

.. raw:: html

   </div>

Core Types
~~~~~~~~~~

``Clock<Tag>``
^^^^^^^^^^^^^^

The sole user-facing handle for a clock domain. A cheaply copyable value type — all
copies share the same backend instance via a ``shared_ptr``. Its API surface is
intentionally uniform across all domains: ``Now()`` for reading, ``Subscribe`` /
``Unsubscribe`` for events, ``Init`` / ``IsAvailable`` / ``WaitUntilAvailable`` for
readiness — with opt-in capabilities gated at compile time by the hook templates.

``ClockTraits<Tag>``
^^^^^^^^^^^^^^^^^^^^

The domain registration point. The primary template is intentionally incomplete; each
clock domain provides a full explicit specialisation that binds together the backend
type, duration, timepoint, snapshot, and the ``CallNow`` factory function. No existing
file is modified when a new domain is added.

``ClockSnapshot<TimepointT, StatusT>``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The immutable return value of every ``Now()`` call. Bundles the timepoint and its
quality metadata into a single atomic read — no separate status call is ever needed.
``TimepointT`` is ``std::chrono::time_point<Tag, Duration>``, making different clock
domains' timepoints incompatible types so cross-domain arithmetic is a compile error.
``StatusT`` is the domain's chosen metadata type.

``StatusT``, ``ClockStatus<FlagEnumT>``, and ``NoStatus``
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

All domain-specific metadata lives in ``StatusT`` — ``ClockSnapshot`` itself is never
extended. Two building blocks are provided: ``NoStatus`` (zero-size placeholder for
always-ready clocks with no quality concept) and ``ClockStatus<FlagEnumT>`` (generic
bitmask over a scoped flag enum). A domain may use either alone or compose them inside
a richer struct alongside continuous fields — as ``VehicleTimeStatus`` does with
``ClockStatus<VehicleTime::StatusFlag>`` and ``double rate_deviation``.

Capability Hooks
^^^^^^^^^^^^^^^^

Three SFINAE hook templates gate the optional capabilities of ``Clock<Tag>``. Each
primary template is intentionally undefined — using an ungated capability on a domain
that has not opted in is a **compile error**, not a runtime failure:
``InitializationHook<Tag>`` unlocks ``Init()``, ``AvailabilityHook<Tag>`` unlocks
``IsAvailable()`` and ``WaitUntilAvailable()``, and ``SubscriptionHook<Tag, EventType>``
unlocks ``Subscribe`` / ``Unsubscribe`` for a specific event type.

Design Decisions
----------------

Single Entry Point — No Factory Classes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Classical time APIs expose a factory or manager object that clients instantiate and
configure before reading time (e.g. ``TimeBaseManager tm; tm.GetCurrentTime(kVehicleBase)``).
``score::time`` removes that level of indirection: ``Clock<Tag>::GetInstance()`` is the
sole entry point, and the production backend is chosen at **link time** by the Bazel
alias target.

Compile-Time Domain Selection Over Runtime Integer Selector
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``score::time`` addresses the same problem domain as time-base management modules found
in automotive middleware stacks: reading a time snapshot, inspecting synchronization
quality flags, waiting for clock availability, and subscribing to synchronization events.

The key design upgrade over typical C-style automotive APIs is replacing the **runtime
integer time-base selector** with a **compile-time ``Tag`` template parameter**. This
gives full type-safety and zero runtime dispatch for time-domain selection: a component
that depends on ``Clock<HighResSteadyTime>`` simply cannot accidentally read
``VehicleTime`` at runtime — the compiler enforces the distinction. All other
structural concepts (composite snapshot result, quality status flags, layered backend
hiding) follow the same principles as established automotive time synchronization
practice, expressed in modern C++.

Opacity of ``details/``
~~~~~~~~~~~~~~~~~~~~~~~

Virtual dispatch exists solely to enable GMock test doubles. The vtable is hidden inside
``details/`` — public headers never declare a virtual function. ``Clock<Tag>`` is a plain
value type. The ``*_mock.h`` headers are the only public headers permitted to include
``details/`` internals.

``ClockSnapshot`` — Immutable Composite Result
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Classical time APIs return a raw timestamp and require a separate call to retrieve the
synchronization status, or expose a struct with public mutable data members and
raw-integer constructors. ``ClockSnapshot<TimepointT, StatusT>`` is a simple
immutable two-field struct:

.. code-block:: cpp

   auto snap = VehicleClock::GetInstance().Now();
   snap.TimePoint();   // std::chrono::time_point<VehicleTime, nanoseconds>
   snap.Status();      // VehicleTimeStatus — returned by value

Generic code works for all clock domains:

.. code-block:: cpp

   template <typename Tag>
   auto Age(score::time::Clock<Tag>& clk,
            typename score::time::Clock<Tag>::time_point ref)
   {
       return clk.Now().TimePoint() - ref;
   }

``Subscribe<E>`` — Uniform Subscription API
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Classical event-callback APIs require a separate named setter and unsetter for each event
type (e.g. ``SetSyncDataCallback``, ``UnsetSyncDataCallback``, ``SetPDelayCallback``,
``UnsetPDelayCallback``). ``Clock<Tag>`` exposes a single pair ``Subscribe<E>()`` /
``Unsubscribe<E>()`` templated on the event type.
The ``SubscriptionHook<Tag, EventType>`` specialisation bridges to the named virtual methods
on the backend interface — which must remain non-template (C++ forbids virtual templates).

Extending with a New Clock Domain
----------------------------------

Adding a new time domain (e.g. ``SdatTime``) requires only new files — no existing file
is modified:

1. Create ``score/time/sdat_time/sdat_time.h`` — tag struct with ``Duration`` and
   ``Timepoint``, and a domain-specific ``SdatTimeStatus`` struct containing whatever
   metadata the backend needs to expose (flags via ``ClockStatus<FlagEnumT>``, continuous
   fields, or both).
2. Create ``score/time/sdat_time/details/sdat_time_iface.h`` — pure-virtual backend interface.
3. Create ``score/time/sdat_time/details/sdat_prod_impl.cpp`` — production backend.
4. Add ``ClockTraits<SdatTime>`` specialisation in ``score/time/sdat_time/sdat_clock.h``.
5. Create ``score/time/sdat_time/sdat_clock_mock.h`` — GMock test double.
6. Add ``sdat_time``, ``sdat_time_mock``, ``interface`` aliases in ``score/time/sdat_time/BUILD``.
7. *(If the new domain requires explicit initialisation)* Add a full specialisation of
   ``InitializationHook<SdatTime>`` in ``sdat_clock.h`` supplying
   ``static bool CallInit(Backend&) noexcept``. This makes ``Clock<SdatTime>::Init()``
   available at compile time without touching any existing files.
8. *(If the new domain requires readiness checking)* Add a full specialisation of
   ``AvailabilityHook<SdatTime>`` in ``sdat_clock.h`` supplying
   ``static bool CallIsAvailable(const Backend&)`` and
   ``static bool CallWaitUntilAvailable(const Backend&, stop_token, time_point)``.
   This makes ``IsAvailable()`` and ``WaitUntilAvailable()`` available at compile time.

Use Cases
---------

VehicleTime
~~~~~~~~~~~

VT1 — Time Polling with Status Check
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Obtain a snapshot and inspect the synchronization quality before using the time value.
``Now()`` returns a single immutable ``ClockSnapshot`` — the timepoint and its
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

+---------------------------+--------------------------------------------------------------+
| Flag                      | Meaning                                                      |
+===========================+==============================================================+
| ``kSynchronized``         | Synchronized at least once to the PTP Grand Master           |
+---------------------------+--------------------------------------------------------------+
| ``kTimeOut``              | No sync message received within the configured time window   |
+---------------------------+--------------------------------------------------------------+
| ``kTimeLeapFuture``       | A large forward adjustment was applied                       |
+---------------------------+--------------------------------------------------------------+
| ``kTimeLeapPast``         | A large backward adjustment was applied                      |
+---------------------------+--------------------------------------------------------------+

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

VT2 — Initialization and Readiness Check
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``VehicleTime`` requires an explicit ``Init()`` call to open the IPC channel to the
:ref:`time_daemon_detailed_design` before any time data becomes available.  Until ``Init()`` returns ``true``,
``Now()`` returns a snapshot with no flags set (``IsConsistent()`` returns ``false``) and ``IsAvailable()`` returns
``false``.

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

VT3 — Async PTP Protocol Data Subscription
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

Delivery is performed by a dedicated worker thread owned by the ``VehicleTime`` backend.
The TimeDaemon publishes into a shared-memory segment without a notification facility, so
the worker polls that segment at a fixed interval (50 ms) while at least one callback is
registered and dispatches every frame whose sync or pDelay content changed.  The frame that
is already present at registration time forms the baseline and is not delivered.

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
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

VT5 — Status Flag Inspection
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

HighResSteadyTime
~~~~~~~~~~~~~~~~~

HT1 — Time Polling
^^^^^^^^^^^^^^^^^^^

Measure a short code-path latency or compute a tight deadline where call overhead matters.
``HighResSteadyClock`` avoids a kernel call on QNX by reading the hardware cycle counter
directly — the same ``Now()`` snapshot pattern used for all clock domains, with no status
check required.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/high_res_steady_time/ht1_polling.puml
   :alt: HT1 — HighResSteadyTime time polling

.. raw:: html

   </div>

.. code-block:: cpp

   #include "score/time/high_res_steady_time/src/high_res_steady_clock.h"
   #include <chrono>

   void MyValidator::CheckDeadline()
   {
       auto hirs = score::time::HighResSteadyClock::GetInstance();
       const auto deadline = hirs.Now().TimePoint() + std::chrono::seconds{3};

       // ... do work ...

       if (hirs.Now().TimePoint() > deadline) {
           HandleDeadlineExceeded();
       }
   }

SteadyClock
~~~~~~~~~~~

ST1 — Time Polling
^^^^^^^^^^^^^^^^^^^

Measure elapsed time between two points, or derive a deadline, using a clock that is
guaranteed never to go backward regardless of external time adjustments.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/steady_clock/st1_polling.puml
   :alt: ST1 — SteadyClock time polling

.. raw:: html

   </div>

.. code-block:: cpp

   #include "score/time/steady_time/src/steady_clock.h"

   void MyComponent::MeasureElapsed()
   {
       auto clock = score::time::SteadyClock::GetInstance();
       const auto start = clock.Now().TimePoint();

       // ... do work ...

       const auto elapsed = clock.Now().TimePoint() - start;
   }

SystemClock
~~~~~~~~~~~

SC1 — Time Polling
^^^^^^^^^^^^^^^^^^^

Record a wall-clock timestamp for logging or audit trails where the absolute calendar
time matters.  Do not use ``SystemClock`` for elapsed time or timeouts — the timepoint
may jump.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/system_clock/sc1_polling.puml
   :alt: SC1 — SystemClock time polling

.. raw:: html

   </div>

.. code-block:: cpp

   #include "score/time/system_time/src/system_clock.h"
   #include <chrono>

   void MyLogger::LogEvent()
   {
       auto clock = score::time::SystemClock::GetInstance();
       const auto wall_time = clock.Now().TimePoint();
       const auto t = std::chrono::system_clock::to_time_t(wall_time);
       LOG_INFO("Event at: {}", std::ctime(&t));
   }

Testing (All Domains)
~~~~~~~~~~~~~~~~~~~~~

Both test utilities work with any clock domain.  Choose based on how the SUT obtains the
clock:

+------------------------------+-------------------------------------------------------+
| Utility                      | When to use                                           |
+==============================+=======================================================+
| ``ScopedClockOverride<Tag>`` | SUT calls ``Clock<Tag>::GetInstance()`` internally.   |
|                              | Scope-bound RAII guard — automatically restored on    |
|                              | destruction.                                          |
+------------------------------+-------------------------------------------------------+
| ``ClockTestFactory<Tag>``    | SUT accepts ``Clock<Tag>`` as a constructor argument. |
|                              | No global state is touched — safe for parallel tests. |
+------------------------------+-------------------------------------------------------+

T1 — ScopedClockOverride (Scope-Bound Override)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/testing/t1_scoped_override.puml
   :alt: T1 — ScopedClockOverride

.. raw:: html

   </div>

.. warning::

   ``ScopedClockOverride`` modifies a **process-wide singleton**. Any ``cc_test`` target that
   uses it must declare ``tags = ["exclusive", "unit"]`` in its Bazel BUILD file. Without
   ``"exclusive"``, Bazel may run multiple tests in the same process shard in parallel, causing
   one test's mock to corrupt another test's clock state and producing flaky failures.

   .. code-block:: python

      cc_test(
          name = "my_service_test",
          srcs = ["my_service_test.cpp"],
          tags = ["exclusive", "unit"],
          deps = [...],
      )

   If the SUT receives the clock via constructor injection instead, use
   ``ClockTestFactory`` (T2) — it does **not** touch the global singleton and
   requires no special tag.

.. code-block:: cpp

   #include "score/time/vehicle_time/src/vehicle_clock.h"
   #include "score/time/vehicle_time/src/vehicle_clock_backend_mock.h"

   TEST(MyServiceTest, ReportsReliableTime)
   {
       auto mock = std::make_shared<score::time::VehicleClockBackendMock>();
       EXPECT_CALL(*mock, Now()).WillOnce(Return(/* snapshot */));

       const score::time::test_utils::ScopedClockOverride<score::time::VehicleTime> guard{mock};

       MyService svc;
       svc.DoSomething();  // calls VehicleClock::GetInstance() internally
   }

T2 — ClockTestFactory (Constructor Injection)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/testing/t2_test_factory.puml
   :alt: T2 — ClockTestFactory

.. raw:: html

   </div>

.. code-block:: cpp

   #include "score/time/vehicle_time/src/vehicle_clock.h"
   #include "score/time/vehicle_time/src/vehicle_clock_backend_mock.h"

   TEST(MyServiceTest, ReportsReliableTime)
   {
       auto mock = std::make_shared<score::time::VehicleClockBackendMock>();
       EXPECT_CALL(*mock, Now()).WillOnce(Return(/* snapshot */));

       const auto clock =
           score::time::test_utils::ClockTestFactory<score::time::VehicleTime>::Make(mock);

       MyService svc{clock};
       svc.DoSomething();
   }

Bazel Dependencies
------------------

Choose the target that matches your use case:

.. list-table::
   :header-rows: 1
   :widths: 55 45

   * - Target
     - When to use
   * - ``//score/time/vehicle_time:vehicle_time``
     - Production binary — includes real PTP backend
   * - ``//score/time/vehicle_time:vehicle_time_mock``
     - Unit test — ``VehicleClockBackendMock`` + scope-bound override or constructor injection
   * - ``//score/time/clock:clock_test_utils``
     - Test utilities — ``ScopedClockOverride`` and ``ClockTestFactory`` (``testonly``;
       must not appear in production deps).  Tests using ``ScopedClockOverride`` must
       also add ``tags = ["exclusive", "unit"]`` to their ``cc_test`` target; tests
       using ``ClockTestFactory`` (constructor injection) do not need this tag.
   * - ``//score/time/vehicle_time:interface``
     - Header-only, no backend — interface/type usage only; required when subscribing
       to ``VehicleTimeStatus`` (provides the type definition)
   * - ``//score/time/high_res_steady_time:high_res_steady_time``
     - Production binary — HIRS steady clock
   * - ``//score/time/high_res_steady_time:high_res_steady_time_mock``
     - Unit test — ``HighResSteadyClockBackendMock`` + scope-bound override or constructor injection
   * - ``//score/time/high_res_steady_time:interface``
     - Header-only
   * - ``//score/time/steady_time:steady_time``
     - ``std::chrono::steady_clock`` wrapper
   * - ``//score/time/system_time:system_time``
     - ``std::chrono::system_clock`` wrapper
   * - ``//score/time/ptp:ptp_types``
     - PTP notification data types (``TimeSlaveSyncData``, ``PDelayMeasurementData``)

Inspection Checklist
--------------------

The checklist for verification of the detailed design and code can be found here:

.. toctree::

   chklst_impl_inspection
