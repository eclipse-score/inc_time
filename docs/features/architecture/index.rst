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

Time Feature Architecture
=========================

.. document:: Time Feature Architecture
   :id: doc__score_time_feat_architecture
   :status: valid
   :version: 1
   :safety: ASIL_B
   :security: YES
   :realizes: wp__feature_arch[version==1]

This page defines the static and dynamic architecture of the :need:`feat__time` feature
(:term:`score::time`). The feature overview, logical interfaces, and requirements are
described in the `platform Time feature documentation
<https://eclipse-score.github.io/score/main/features/time/index.html>`_.

Static Architecture
-------------------

The feature-wide static view shows the Time feature, the three clock interfaces
it exposes, and the SW components that implement them:

* **score::time** (clock library) — ``Clock<Tag>`` API exposing
  :term:`Vehicle Clock`, :term:`Local Clock`, and :term:`Absolute Clock`
* **TimeDaemon** — time validation, aggregation and distribution process
* **TimeSlave** — gPTP slave endpoint that receives network time
* **ts_client** — SW component implementing the gPTP shared-memory IPC channel
  between ``TimeSlave`` (publisher) and ``TimeDaemon`` (reader)

.. feat_arc_sta:: Time Static Architecture
   :id: feat_arc_sta__time__static_view
   :security: YES
   :safety: ASIL_B
   :status: valid
   :version: 1
   :includes: logic_arc_int__time__vehicle_clock, logic_arc_int__time__local_clock, logic_arc_int__time__absolute_clock
   :fulfils: feat_req__time__mocking_apis[version==1], feat_req__time__vehicle_time_sync[version==1], feat_req__time__abs_sync[version==1]
   :belongs_to: feat__time[version==1]

   .. uml:: _assets/static_arch.puml
      :scale: 50
      :align: center

Dynamic Architecture
--------------------

Vehicle Time
************

The :term:`Vehicle Clock` exposes two runtime interaction modes: a **pull** model — reading the
latest accumulated snapshot on demand — and an **event notification** model — subscribing to
receive notifications when the :term:`Vehicle Time status` changes or new time-sync relevant
data from the :term:`Time slave` arrives.

.. rubric:: Reading the time

Applications read the current vehicle time through the :term:`Vehicle Clock` (``now``). In the
background the time is continuously synchronized to the external network time
(:term:`gPTP`), validated, and accumulated as a snapshot. The read uses the latest
accumulated snapshot and interpolates it on the current local monotonic clock — a fast local
operation that does not cross the process boundary.

.. feat_arc_dyn:: Vehicle Time Read
   :id: feat_arc_dyn__time__vehicle
   :security: NO
   :safety: ASIL_B
   :status: valid
   :version: 1
   :fulfils: feat_req__time__vehicle_time_ctrl_flow[version==1]
   :belongs_to: feat__time[version==1]

   .. uml:: _assets/vehicle_time_read_flow.puml
      :scale: 50
      :align: center

.. rubric:: Receiving notifications

Applications can subscribe to :term:`Vehicle Clock` events. Subscription is the only way to obtain
the PTP payload data (the sync/follow-up and pdelay sequences, which are independent). The primary
use case is diagnostics — reacting to a status change or a PTP data update in a diagnostic manner.
The :term:`Vehicle Time status` can also be read from ``now``, and subscription additionally
delivers it when it changes.

.. feat_arc_dyn:: Vehicle Time Notification
   :id: feat_arc_dyn__time__vehicle_subscription
   :security: NO
   :safety: ASIL_B
   :status: valid
   :version: 1
   :fulfils: feat_req__time__vehicle_time_sync_log[version==1]
   :belongs_to: feat__time[version==1]

   .. uml:: _assets/vehicle_time_notification.puml
      :scale: 50
      :align: center

Local Time
**********

All three local clock domains share the same ``now`` interface and resolve directly to an OS
clock read — no initialization or background synchronization is required. The domain is selected
at construction time and is always available.

.. rubric:: Reading the time

.. feat_arc_dyn:: Local Time Read
   :id: feat_arc_dyn__time__local
   :security: NO
   :safety: ASIL_B
   :status: valid
   :version: 1
   :fulfils: feat_req__time__monotonic_clock_api[version==1], feat_req__time__high_prec_clock_api[version==1]
   :belongs_to: feat__time[version==1]

   .. uml:: _assets/local_time_read_flow.puml
      :scale: 50
      :align: center

Absolute Time
*************

Absolute time is delivered from an external time master through the ``score::someip`` stack. Each
sample carries a :term:`Delay Tag` so the transmission delay can be compensated. The
:term:`score::time` feature converts the :term:`Delay Tag` to a local monotonic tag before
publishing, so the :term:`Absolute Clock` compensates the delay on the local monotonic base —
preserving precision without making clients depend on the :term:`Vehicle Clock`.

.. rubric:: Reading the time

.. feat_arc_dyn:: Absolute Time Read
   :id: feat_arc_dyn__time__absolute
   :security: YES
   :safety: ASIL_B
   :status: valid
   :version: 1
   :fulfils: feat_req__time__abs_base_api[version==1], feat_req__time__abs_sync[version==1]
   :belongs_to: feat__time[version==1]

   .. uml:: _assets/absolute_time_read_flow.puml
      :scale: 50
      :align: center

Mocking Support
---------------

The :term:`score::time` feature provides a mockable interface for each clock domain —
:term:`Vehicle Clock`, :term:`Local Clock` and :term:`Absolute Clock`. Application developers
can substitute any clock interface with a controlled implementation during testing, enabling
deterministic unit, component and integration tests of time-dependent code without requiring
a real time source (:need:`feat_req__time__mocking_apis`).

.. feat_arc_dyn:: Clock Mocking
   :id: feat_arc_dyn__time__mocking
   :security: NO
   :safety: ASIL_B
   :status: valid
   :version: 1
   :fulfils: feat_req__time__mocking_apis[version==1]
   :belongs_to: feat__time[version==1]

   .. uml:: _assets/clock_testability.puml
      :scale: 50
      :align: center
