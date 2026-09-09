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

.. _time_slave_user_manual:

Time Slave User Manual
######################

.. document:: User Manual Time Slave Component
   :id: doc__user_manual_time_slave
   :status: draft
   :version: 1
   :safety: QM
   :security: NO
   :realizes: wp__training_path[version==1]

Overview
========

The Time Slave component is a system daemon responsible for synchronizing with the PTP Grandmaster Clock over the network. It adjusts the hardware clock (PHC) and publishes synchronization data to shared memory for consumption by the ``TimeDaemon``.

For module-level integration and deployment information, see the main module manual.

Configuration
=============

.. toctree::
   :maxdepth: 2

   config/configuration_guide

Runtime Requirements
====================

Operating System Privileges
---------------------------

The ``TimeSlave`` executable (``time_slave``) requires elevated privileges to access raw network sockets and control the hardware clock. It is strongly recommended **not** to run this process as the `root` user. Instead, grant the required Linux Capabilities to the executable:

.. code-block:: bash

   sudo setcap cap_net_admin,cap_net_raw,cap_sys_time+eip /path/to/time_slave

*   ``cap_net_admin``: For network interface configuration.
*   ``cap_net_raw``: For the use of raw sockets to listen to PTP traffic.
*   ``cap_sys_time``: For adjusting the system's hardware clock.

Network Requirements
--------------------

*   The network interface used for PTP communication **must** be provided via the ``-i, --interface <name>`` command-line argument.
*   The ECU must have network connectivity to the PTP Grandmaster clock on this interface.
*   Network hardware must support PHC (PTP Hardware Clock).

Platform-Specific Setup
=======================

.. toctree::
   :maxdepth: 1

   qnx_setup
