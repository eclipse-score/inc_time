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

Component Time Requirements
###########################

.. document:: Time Requirements
   :id: doc__time_requirements
   :status: valid
   :version: 1
   :safety: QM
   :security: NO
   :realizes: wp__requirements_comp[version==1]
   :tags: requirements, time

Functional Requirements
-----------------------

.. comp_req:: Unified clock facade across time domains
   :id: comp_req__time__unified_clock_facade
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__high_prec_clock_api, feat_req__time__monotonic_clock_api, feat_req__time__abs_base_api, feat_req__time__vehicle_time_time_api
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall provide a single type-safe accessor for all supported
   clock domains that prevents mixing time values from different domains by
   rejecting domain-mismatched operations at compile time when accessing
   time snapshots.

.. comp_req:: Immutable snapshot with quality metadata
   :id: comp_req__time__snapshot_with_status
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_time_api
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall return a single immutable value that bundles
   the timepoint with the domain's synchronization and quality
   metadata, so callers observe state that cannot change after
   creation and can determine time validity without a separate
   status query.

.. comp_req:: Explicit lifecycle for backends that need it
   :id: comp_req__time__explicit_lifecycle
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_ctrl_flow
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall provide initialization, availability-check, and
   availability-wait operations for clock domains that depend on external
   resources, and shall cause compilation failure when those operations
   are invoked on clock domains that are always ready.

.. comp_req:: VehicleClock returns snapshot with status
   :id: comp_req__vehicle_time__snapshot
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_time_api
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall read the vehicle time timepoint and its status
   atomically from the backend within a single backend operation, so
   downstream callers observe consistent time and status values.

.. comp_req:: VehicleClock lifecycle operations
   :id: comp_req__vehicle_time__lifecycle
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_ctrl_flow
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall delegate initialization and
   availability checks to the backend and shall report backend
   init failure and availability-wait timeouts to the caller without
   blocking indefinitely.

.. comp_req:: HighResSteadyClock always-ready snapshot
   :id: comp_req__high_res_steady_time__snapshot
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__high_prec_clock_api
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall provide monotonic high-resolution time snapshots
   without requiring prior initialization.

.. comp_req:: SteadyClock always-ready snapshot
   :id: comp_req__steady_time__snapshot
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__monotonic_clock_api
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall provide steady time snapshots without
   requiring prior initialization, and shall cause compilation failure
   when initialization or availability operations are invoked on the
   steady time domain.

.. comp_req:: SystemClock always-ready snapshot
   :id: comp_req__system_time__snapshot
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__abs_base_api
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall provide system time snapshots without
   requiring prior initialization, and shall cause compilation failure
   when initialization or availability operations are invoked on the
   system time domain.

.. comp_req:: Supported platforms
   :id: comp_req__time__supported_platforms
   :reqtype: Non-Functional
   :security: NO
   :safety: QM
   :status: valid
   :version: 1
   :satisfied_by: comp__time

   The Component shall build and run on Linux and QNX host platforms,
   providing platform-specific backends where the underlying OS APIs
   differ.

Assumption of Use Requirements
------------------------------

.. aou_req:: Backend initialization by the user
   :id: aou_req__time__user_initializes_backend
   :reqtype: Process
   :security: NO
   :safety: QM
   :status: valid
   :version: 1

   The Component User shall initialize the backends for clock domains
   that require initialization before requesting time snapshots
   from those domains.


.. needextend:: "c.this_doc()"
   :+tags: time

.. toctree::
   :maxdepth: 1

   chklst_req_inspection
