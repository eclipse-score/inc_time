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

.. _time_slave:

Time Slave
##########

.. document:: Time Slave
   :id: doc__time_slave
   :status: valid
   :version: 1
   :safety: QM
   :security: NO
   :realizes: wp__cmpt_request
   :tags: time_slave

.. comp:: Time Slave
   :id: comp__time_slave
   :security: NO
   :safety: QM
   :status: valid
   :version: 1
   :belongs_to: feat__time[version==1]


Abstract
========

This component implements a gPTP (IEEE 802.1AS) time synchronization slave daemon that receives time synchronization data from network and sends it via ts_client to the time_daemon for validation.

Specification
=============

The time_slave component provides IEEE 802.1AS synchronization handling, i.e. it receives gPTP network messages from time master, does path delay measurement, computes local clock correction values, and publishes synchronized time snapshots via ts_client shared memory IPC.

Runtime flow follows continuous loop:

1. **Initialization and setup**: Bind configured network interface, initialize gPTP engine and IPC publisher, enable hardware timestamping when platform supports it.
2. **Protocol processing**: Filter by configured gPTP domain, process Sync and Follow_Up pairs, compute offset and peer delay from protocol timestamps and correction fields.
3. **Clock adjustment and status tracking**: Apply PHC step or frequency adjustment based on offset behavior, detect synchronization timeout and forward/backward time leaps.
4. **Publishing**: Publish synchronized time snapshot, peer delay, rate deviation, and status flags to IPC at fixed 50 millisecond interval.

Key Behaviors
-------------

**Timestamping Strategy**: Hardware timestamping preferred; software timestamping fallback used when hardware capabilities are unavailable.

**Clock Control Strategy**: Large offset handled by step adjustment; stable offset evolution handled by frequency slew adjustment.

**Fault and Quality Flags**: Timeout status set when no valid Sync/Follow_Up pair is processed within configured timeout window. Time leap future and time leap past flags track forward and backward master time discontinuities.

**Diagnostics**: Optional runtime instrumentation can record synchronization events into CSV output when diagnostics option is enabled.

**Platform Support**: Linux and QNX 8.0 SDP supported for raw Ethernet, timestamp acquisition, and PHC control.

**Error Reporting**: Initialization, network, protocol, and timestamping fallback events are logged via score::mw::log.

Assumptions of Use
------------------

Users must configure and bring up network interface before starting time_slave. Exactly one time_slave instance per network interface is supported.

Footnotes
=========

Further Documentation of the component can be found in the following sections:

Component Detail Information
============================

.. toctree::
   :maxdepth: 1

   manuals/user_manual
   detailed_design/index
   requirements/index
