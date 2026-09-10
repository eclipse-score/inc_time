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

.. _time_daemon:

Time Daemon
###########

.. document:: Time Daemon
   :id: doc__time_daemon
   :status: draft
   :version: 1
   :safety: ASIL_B
   :security: NO
   :realizes: wp__cmpt_request
   :tags: time_daemon

.. comp:: Time Daemon
   :id: comp__time_daemon
   :security: NO
   :safety: ASIL_B
   :status: valid
   :version: 1
   :belongs_to: feat__time

Abstract
========

This component implements a time synchronization daemon that receives time data from time_slave via IPC, performs verification and qualification, and provides validated time information to applications.

Specification
=============

The time_daemon component acts as a verification and publishing layer between the time_slave component and client applications. It receives gPTP time synchronization data from shared memory, validates it through multiple verification stages, and publishes qualified time data via IPC.

The component operates as a continuous loop with the following stages:

1. **Data Reception**: Reads gPTP time synchronization data from shared memory written by time_slave
2. **Verification Pipeline**: Validates data through three checks:

   * Synchronization status validation
   * Time jump detection (>500μs between consecutive frames)
   * Timeout detection (no new data within 3.3 seconds)

3. **Publishing**: Publishes verified time data with quality indicators to clients via VehicleTime IPC interface at a fixed 250ms interval

Key Behaviors
-------------

**Startup Stabilization**: Synchronization state changes are not reported during the first 5 seconds after initial synchronization to avoid spurious time jump detection.

**Error Recovery**: Time jump and timeout conditions are non-fatal. The component continues publishing with appropriate status flags set. Time jump condition clears after 2 consecutive valid frames.

**Multi-Client Support**: Multiple client applications can concurrently read published time data.

**Platform Support**: Linux and QNX 8.0 SDP platforms supported for shared memory and IPC operations.

Assumptions of Use
------------------

The gPTP shared memory must be available and initialized before synchronized time data published by time_daemon is relied upon. Otherwise shared memory access failures or stale data may occur.

Footnotes
=========

Further Documentation of the component can be found in the following sections:

Component Detail Information
============================

.. toctree::
   :maxdepth: 1

   detailed_design/index
   requirements/index
   manuals/user_manual
