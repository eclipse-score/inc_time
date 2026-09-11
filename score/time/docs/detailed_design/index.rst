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

Units within the Component
--------------------------

The relationship between a unit and its parent component is established implicitly
through the file path. Each component has its own directory, and units residing
within that directory belong to it. The unit's attributes and behaviour are documented
in the source code itself.

- **Clock<Tag>**: Uniform API entry point for all clock domains (see ``clock.h``)
- **ClockTraits<Tag>**: Domain registration point (see ``clock.h``)
- **ClockSnapshot<TimepointT, StatusT>**: Immutable composite return value (see ``clock_snapshot.h``)
- **ClockStatus<FlagEnumT>**: Generic status flag container (see ``clock_status.h``)
- **NoStatus**: Zero-size status placeholder for always-ready clocks (see ``clock.h``)
- **VehicleClockBackend**: Backend interface for VehicleTime domain (see ``vehicle_time/src/vehicle_clock_backend.h``)
- **HighResSteadyClockBackend**: Backend interface for HighResSteadyTime domain (see ``high_res_steady_time/src/high_res_steady_clock_backend.h``)

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

Adding a new time domain (e.g. ``SdatTime``) requires only new files — no existing file is modified:

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

See ``score/time/vehicle_time/`` and ``score/time/high_res_steady_time/`` for reference implementations.

Usage Examples
--------------

Working code examples for all clock domains are available in ``examples/time/``:

**VehicleTime** (``examples/time/vehicle_time/``)
  - Time polling with status check (Now() + IsReliable())
  - Initialization and readiness check (Init())
  - Synchronization status subscription (Subscribe<VehicleTimeStatus>)
  - Testing with ScopedClockOverride and GMock
  - Status flag inspection (IsFlagActive, IsReliable, HasBeenSynchronized)

**HighResSteadyTime** (``examples/time/high_res_steady_time/``)
  - High-resolution monotonic time polling
  - Testing with ScopedClockOverride

**SteadyClock** (``examples/time/steady_time/``)
  - Standard monotonic timing (elapsed time, timeouts)

**SystemClock** (``examples/time/system_time/``)
  - Wall-clock time for logging
  - Testing with ScopedClockOverride

Each example includes buildable source code with unit tests demonstrating API usage and testing patterns.

For comprehensive usage guidance, see :doc:`User Manual </module/manuals/user_manual>`.

Bazel Dependencies
------------------

.. list-table::
   :header-rows: 1
   :widths: 55 45

   * - Target
     - When to use
   * - ``//score/time/vehicle_time:vehicle_time``
     - Production binary — includes PTP backend
   * - ``//score/time/vehicle_time:vehicle_time_mock``
     - Unit test — GMock test double
   * - ``//score/time/clock:clock_test_utils``
     - Test utilities — ``ScopedClockOverride`` / ``ClockTestFactory`` (``testonly``)
   * - ``//score/time/vehicle_time:interface``
     - Header-only — type definitions, no backend
   * - ``//score/time/high_res_steady_time:high_res_steady_time``
     - Production binary — high-resolution steady clock
   * - ``//score/time/high_res_steady_time:high_res_steady_time_mock``
     - Unit test — GMock test double
   * - ``//score/time/steady_time:steady_time``
     - ``std::chrono::steady_clock`` wrapper
   * - ``//score/time/system_time:system_time``
     - ``std::chrono::system_clock`` wrapper
   * - ``//score/time/ptp:ptp_types``
     - PTP notification data types

Inspection Checklist
--------------------

The checklist for verification of the detailed design and code can be found here:

.. toctree::

   chklst_impl_inspection
