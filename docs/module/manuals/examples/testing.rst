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

Testing with Clock Mocks
========================

Overview
--------

Both test utilities work with any clock domain.  Choose based on how the SUT obtains the
clock:

+------------------------------+----------------------------------------------------------+
| Utility                      | When to use                                              |
+==============================+==========================================================+
| ``ScopedClockOverride<Tag>`` | SUT calls ``Clock<Tag>::GetInstance()`` internally.      |
|                              | Scope-bound RAII guard — automatically restored on       |
|                              | destruction.                                             |
+------------------------------+----------------------------------------------------------+
| ``ClockTestFactory<Tag>``    | SUT accepts ``Clock<Tag>`` as a constructor argument.    |
|                              | No global state is touched — safe for parallel tests.    |
+------------------------------+----------------------------------------------------------+

T1 — ScopedClockOverride (Scope-Bound Override)
------------------------------------------------

Use when the system under test calls ``Clock<Tag>::GetInstance()`` internally. The guard
installs a mock backend into the process-wide singleton for the duration of its scope,
then restores the original on destruction.

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

**Single clock domain:**

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

**Multiple clock domains** (as used in the ``vehicle_time`` example):

.. code-block:: cpp

   #include "score/time/clock/src/scoped_clock_override.h"
   #include "score/time/vehicle_time/src/vehicle_clock_backend_mock.h"
   #include "score/time/high_res_steady_time/src/high_res_steady_clock_backend_mock.h"

   class VehicleTimeHandlerTest : public ::testing::Test
   {
     protected:
       VehicleTimeHandlerTest()
           : vehicle_mock_{std::make_shared<score::time::VehicleClockBackendMock>()},
             hirs_mock_{std::make_shared<score::time::HighResSteadyClockBackendMock>()},
             vehicle_guard_{vehicle_mock_},
             hirs_guard_{hirs_mock_}
       {
       }

       std::shared_ptr<score::time::VehicleClockBackendMock> vehicle_mock_;
       std::shared_ptr<score::time::HighResSteadyClockBackendMock> hirs_mock_;
       score::time::test_utils::ScopedClockOverride<score::time::VehicleTime> vehicle_guard_;
       score::time::test_utils::ScopedClockOverride<score::time::HighResSteadyTime> hirs_guard_;
   };

   TEST_F(VehicleTimeHandlerTest, ReportContainsSynchronizedVehicleTime)
   {
       EXPECT_CALL(*vehicle_mock_, Now()).WillOnce(Return(/* vehicle_snapshot */));
       EXPECT_CALL(*hirs_mock_,    Now()).WillOnce(Return(/* hirs_snapshot */));

       VehicleTimeHandler handler;
       const auto report = handler.GetCurrentTime();
       EXPECT_TRUE(report.is_reliable);
   }

The full working example is in
``examples/time/vehicle_time/src/vehicle_time_handler_test.cpp``.

T2 — ClockTestFactory (Constructor Injection)
----------------------------------------------

Use when the SUT accepts ``Clock<Tag>`` as a constructor argument.  No global singleton
is touched — safe to run in parallel without ``"exclusive"`` tag.

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
       must not appear in production deps).  Tests using ``ScopedClockOverride``
       must also add ``tags = ["exclusive", "unit"]`` to their ``cc_test`` target;
       tests using ``ClockTestFactory`` (constructor injection) do not need this tag.
   * - ``//score/time/vehicle_time:interface``
     - Header-only, no backend — interface/type usage only; required when subscribing
       to ``VehicleTimeStatus`` (provides the type definition)
