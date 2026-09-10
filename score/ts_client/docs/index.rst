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

.. _ts_client:

Time Sync Client
################

.. document:: Time Sync Client
   :id: doc__ts_client
   :status: draft
   :version: 1
   :safety: ASIL_B
   :security: NO
   :realizes: wp__cmpt_request
   :tags: ts_client

.. comp:: Time Sync Client
   :id: comp__ts_client
   :security: NO
   :safety: ASIL_B
   :status: valid
   :version: 1
   :belongs_to: feat__time

Abstract
========

This component encapsulates the IPC mechanisms for time synchronization data exchange between the components time_slave and time_daemon.

Specification
=============

The ts_client component provides shared memory IPC channel for gPTP time synchronization data exchange between one publisher and multiple readers within ECU.

Channel lifecycle follows producer-consumer model:

1. **Channel setup**: Publisher creates shared memory channel and Receiver opens existing channel in read-only mode.
2. **Validation and synchronization**: Receiver validates shared memory region on open, then reads data through lock-free synchronization with concurrent write detection.
3. **Data exchange**: Channel carries synchronization status, Sync/FollowUp metadata, PDelay metadata, and time correlation data for downstream processing.

Key Behaviors
-------------

**Lock-Free Multi-Reader Access**: Multiple readers can access same shared memory channel concurrently while single publisher writes.

**Data Integrity Handling**: Receiver reports invalid or corrupted reads when integrity checks fail.

**Platform Support**: Shared memory IPC flow supported on Linux and QNX 8.0 SDP.

**Error Reporting**: Shared memory create/open/validation failures are logged via score::mw::log.

**Cache-Aware Layout**: Shared memory layout is optimized to reduce cache contention between writer and readers.

Assumptions of Use
------------------

Only one publisher process may open and write given shared memory segment. Users must configure permissions so publisher has write access and readers have read access.

Footnotes
=========

Further Documentation of the component can be found in the following sections:

Component Detail Information
============================

.. toctree::
   :maxdepth: 1

   detailed_design/index
   requirements/index
