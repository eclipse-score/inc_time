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

Component Time Sync Client Requirements
########################################

.. document:: Time Sync Client Requirements
   :id: doc__ts_client_requirements
   :status: draft
   :version: 1
   :safety: ASIL_B
   :security: NO
   :realizes: wp__requirements_comp[version==1]
   :tags: requirements, ts_client


Functional Requirements
-----------------------

Shared Memory Management
^^^^^^^^^^^^^^^^^^^^^^^^

.. comp_req:: Shared Memory Channel Management
   :id: comp_req__ts_client__shared_memory_mgmt
   :reqtype: Functional
   :security: NO
   :safety: ASIL_B
   :derived_from: feat_req__time__vehicle_time_sync[version==1], feat_req__time__vehicle_time_ctrl_flow[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall provide operations to create, open, and close shared memory channels for gPTP data exchange between time_slave and time_daemon.

.. comp_req:: Shared Memory Region Validation
   :id: comp_req__ts_client__shm_validation
   :reqtype: Functional
   :security: NO
   :safety: ASIL_B
   :derived_from: feat_req__time__vehicle_time_sync[version==1], feat_req__time__vehicle_time_acc_qual_api[version==1], feat_req__time__vehicle_time_time_pt_qual[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall validate shared memory region integrity on Open operations and reject regions that fail validation.

.. comp_req:: Publisher Creates Channels
   :id: comp_req__ts_client__publisher_creates
   :reqtype: Functional
   :security: NO
   :safety: ASIL_B
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client Publisher shall create shared memory channels that Receivers subsequently open.

.. comp_req:: Receiver Multi-Reader Semantics
   :id: comp_req__ts_client__receiver_multi_reader
   :reqtype: Functional
   :security: NO
   :safety: ASIL_B
   :derived_from: feat_req__time__vehicle_time_ctrl_flow[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client Receiver shall support multiple concurrent readers accessing shared memory channels in read-only mode.

.. comp_req:: Data Validity Indication
   :id: comp_req__ts_client__data_validity
   :reqtype: Functional
   :security: NO
   :safety: ASIL_B
   :derived_from: feat_req__time__vehicle_time_sync[version==1], feat_req__time__vehicle_time_acc_qual_api[version==1], feat_req__time__vehicle_time_time_pt_qual[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client Receiver shall indicate whether the shared memory channel is obviously corrupted.

Data Synchronization
^^^^^^^^^^^^^^^^^^^^

.. comp_req:: Lock-Free Synchronization
   :id: comp_req__ts_client__seqlock_protocol
   :reqtype: Functional
   :security: NO
   :safety: ASIL_B
   :derived_from: feat_req__time__vehicle_time_sync[version==1], feat_req__time__vehicle_time_ctrl_flow[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall implement non-blocking synchronization between writer and readers and ensure readers can detect concurrent writes.

Data Exchange Interfaces
^^^^^^^^^^^^^^^^^^^^^^^^^

.. comp_req:: gPTP Synchronization Status Exchange
   :id: comp_req__ts_client__sync_status_data
   :reqtype: Interface
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall exchange gPTP synchronization status information containing synchronized state and timeout condition.

.. comp_req:: Sync/FollowUp Message Metadata Exchange
   :id: comp_req__ts_client__sync_fup_data
   :reqtype: Interface
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall exchange gPTP Sync and FollowUp message metadata containing precise origin timestamp, reference global timestamp, reference local timestamp, sync ingress timestamp, correction field, sequence identifier, path delay, port number, and clock identity.

.. comp_req:: PDelay Message Metadata Exchange
   :id: comp_req__ts_client__pdelay_data
   :reqtype: Interface
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall exchange gPTP Peer Delay (PDelay) message metadata containing request origin timestamp, request receipt timestamp, response origin timestamp, response receipt timestamp, path delay measurement, request port number, response port number, and request clock identity.

.. comp_req:: Time Correlation Data Exchange
   :id: comp_req__ts_client__time_correlation_data
   :reqtype: Interface
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall exchange time correlation data containing synchronized PTP assumed time, local system time reference, and clock rate deviation.

Platform Abstraction
^^^^^^^^^^^^^^^^^^^^

.. comp_req:: Platform Support
   :id: comp_req__ts_client__platform_support
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall support POSIX and QNX 8.0 SDP platforms for shared memory operations and inter-process communication.


Error Handling
^^^^^^^^^^^^^^

.. comp_req:: Error Reporting
   :id: comp_req__ts_client__error_reporting
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync_log[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall log messages via the score::mw::log interface when shared memory operations fail (error level), validation errors occur (error level), or channel creation/opening fails (error level).

Non-Functional Requirements
---------------------------

.. comp_req:: Cache-Optimized Memory Layout
   :id: comp_req__ts_client__cache_optimization
   :reqtype: Non-Functional
   :security: NO
   :safety: ASIL_B
   :derived_from: feat_req__time__vehicle_time_ctrl_flow[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__ts_client

   The ts_client component shall optimize shared memory layout to prevent cache contention between concurrent writer and reader processes.

Assumption of Use Requirements
------------------------------

.. aou_req:: Single Publisher Process
   :id: aou_req__ts_client__single_publisher
   :reqtype: Process
   :security: NO
   :safety: ASIL_B
   :status: valid
   :version: 1

   The ts_client user shall ensure that only one publisher process opens and writes to a shared memory segment to maintain data consistency. Multiple publishers writing to the same segment will cause data corruption and readers will receive invalid data.

.. aou_req:: Shared Memory Permissions
   :id: aou_req__ts_client__shm_permissions
   :reqtype: Process
   :security: NO
   :safety: ASIL_B
   :status: valid
   :version: 1

   The user shall configure system permissions to allow publisher write access and readers read access to shared memory segments. Incorrect permissions will cause channel creation or opening failures.

.. needextend:: "c.this_doc()"
   :+tags: ts_client

.. toctree::
   :maxdepth: 1

   chklst_req_inspection
