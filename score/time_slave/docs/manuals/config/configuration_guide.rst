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

.. _time_slave_configuration:

TimeSlave Configuration
=======================

The behavior of the ``TimeSlave`` is controlled via a JSON configuration
file. The schema is defined in
``score/time_slave/src/application/configuration/time_slave_config_schema.json``
and covers all runtime parameters: network interface, gPTP timing, PHC
adjustment, shared-memory path, and platform-specific options.

Configuration file location
---------------------------

The configuration file is resolved in the following order:

1. ``--config <path>`` command-line argument.
2. ``TIMESLAVE_CONFIG`` environment variable.
3. ``./etc/time_slave_config.json`` relative to the process working
   directory.

If no configuration file is found at the resolved path, TimeSlave uses
built-in defaults (equivalent to the schema defaults) and logs an
informational message.

For backwards compatibility, the ``GPTP_IFACE`` environment variable
(when set) still overrides the ``iface_name`` field after the JSON config
is loaded.

Command-Line Arguments
-----------------------

The following command-line arguments are available to configure the
``TimeSlave`` at runtime:

``--config <path>``
  Path to the JSON configuration file. When provided, it takes precedence
  over the ``TIMESLAVE_CONFIG`` environment variable and the default
  ``./etc/time_slave_config.json`` path. See :ref:`time_slave_configuration`
  for the full resolution order.

Default Configuration (`GptpEngineOptions`)
--------------------------------------------

The following table lists all available options and their default values as defined in the source code. Currently, only ``iface_name`` can be changed without recompiling the application.

.. list-table:: GptpEngineOptions Default Values
   :widths: 25 15 60
   :header-rows: 1

   * - Option
     - Default Value
     - Description
   * - ``iface_name``
     - ``"emac0"``
     - The network interface to use for gPTP traffic.
   * - ``pdelay_interval_ms``
     - ``1000``
     - The interval in milliseconds for sending Peer-Delay measurement requests.
   * - ``pdelay_warmup_ms``
     - ``2000``
     - The initial delay in milliseconds before the first Peer-Delay request is sent.
   * - ``sync_timeout_ms``
     - ``3300``
     - The time in milliseconds without receiving a PTP Sync message before a timeout is declared and the clock is considered unreliable.
   * - ``jump_future_threshold_ns``
     - ``500'000'000``
     - The threshold in nanoseconds (500 ms) for detecting a significant forward time jump.
   * - ``domain_number``
     - ``0``
     - The gPTP domain number. The TimeSlave will only interact with a PTP master in the same domain.
   * - ``phc_config``
     - ``disabled``
     - Configuration for hardware clock (PHC) adjustments. Disabled by default.

PHC configuration
-----------------

To enable PTP Hardware Clock adjustment, set ``"phc"."enabled"`` to
``true``. The ``device`` field identifies the network interface whose
PTP clock is adjusted (e.g. ``"emac0"`` on QNX, ``"eth0"`` on Linux):

.. code-block:: json

   {
       "phc": {
           "enabled": true,
           "device": "emac0",
           "step_threshold_ns": 100000000
       }
   }

When disabled (the default), gPTP runs in slave-only mode without
disciplining a hardware clock.

QNX-specific fields
-------------------

The optional ``"qnx"`` section holds QNX-only settings (ignored on
Linux):

.. code-block:: json

   {
       "qnx": {
           "bpf_device_prefix": "/dev/bpf",
           "see_sent": true
       }
   }

``bpf_device_prefix``
  Path prefix for the BPF (Berkeley Packet Filter) devices used for raw
  Ethernet frame capture. Defaults to ``/dev/bpf``.

``see_sent``
  When ``true``, sent frames are also delivered to the RX BPF. This is
  required to capture TX hardware timestamps for Pdelay_Req T1
  measurement. Defaults to ``false``.

Example Invocation
------------------

.. code-block:: bash

   # Start the TimeSlave with a custom configuration file
   ./time_slave --config etc/time_slave_config.json

   # Use a non-default interface without a config file (backwards compat)
   GPTP_IFACE=emac1 ./time_slave

Example
-------

A representative QNX configuration:

.. code-block:: json

   {
       "iface_name": "emac0",
       "domain_number": 0,
       "pdelay_req_interval_ms": 1000,
       "pdelay_warmup_ms": 2000,
       "sync_timeout_ms": 3300,
       "jump_future_threshold_ns": 500000000,
       "shm_path": "/gptp_shmem",
       "phc": {
           "enabled": true,
           "device": "emac0",
           "step_threshold_ns": 100000000
       },
       "qnx": {
           "bpf_device_prefix": "/dev/bpf",
           "see_sent": true
       }
   }
