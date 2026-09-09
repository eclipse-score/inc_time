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

Running TimeSlave on QNX
########################

This manual explains how to configure, build, and run the TimeSlave
process on QNX (SDP 8.0.4, x86_64).

Prerequisites
*************

* `QNX Software Center <https://www.qnx.com/download/>`_ installed on the
  build host, with **QNX SDP 8.0** deployed.
* A valid QNX license, placed under ``/var/tmp/.qnx/license/licenses``
  (the QNX tools hard-code ``QNX_CONFIGURATION_EXCLUSIVE=/var/tmp/.qnx``).
* `Bazel <https://bazel.build/>`_ 8.6.0 or later. On first QNX build the
  toolchain is downloaded automatically (authenticated via the
  ``qnx.com`` credentials above); no additional toolchain setup script
  is needed.
* QEMU with KVM support for running the reference integration test
  (optional, but required for the QEMU-based integration test).
* Bazel user credentials for ``qnx.com`` (used by
  ``tools/qnx_credential_helper.py``) — set via ``~/.netrc`` or the
  ``SCORE_QNX_USER`` / ``SCORE_QNX_PASSWORD`` environment variables.

Configuration
*************

For general TimeSlave configuration (config file lookup order, JSON
schema reference, command-line arguments, PHC configuration, and
QNX-specific fields), see the
:ref:`time_slave_configuration` section.

Build steps
***********

Build for QNX x86_64
====================

.. code-block:: shell

   # Build TimeSlave for QNX x86_64.
   # The QNX toolchain is downloaded automatically on first build
   # (requires the qnx.com credentials configured above).
   bazel build --config=x86_64-qnx //score/time_slave:time_slave

The resulting binary appears under ``bazel-bin/score/time_slave/src/application/time_slave``.

Packaging for deployment
========================

To produce a deployable layout (``/opt/time_slave/bin/`` +
``/opt/time_slave/etc/``) use the ``pkg_application`` macro defined in
``score/time_slave/pkg_application.bzl``. A typical BUILD snippet:

.. code-block:: python

   load("//score/time_slave:pkg_application.bzl", "pkg_application")

   pkg_application(
       name = "time_slave-pkg",
       app_name = "time_slave",
       bin = ["//score/time_slave:time_slave"],
       etc = ["//score/time_slave/config/qnx:time_slave_qnx_config.json"],
   )

The resulting ``pkg_filegroup`` can be fed to ``pkg_tar`` (for
archive-based deployment), ``oci_image`` (for Docker), or ``qnx_ifs``
(for inclusion in a QNX IFS image).

Running on QNX
**************

On a QNX target (or in the QEMU reference integration), copy the binary
and config to the conventional locations and run:

.. code-block:: shell

   # On the QNX target shell:
   mkdir -p /opt/time_slave/etc
   cp time_slave /opt/time_slave/bin/
   cp time_slave_qnx_config.json /opt/time_slave/etc/time_slave_config.json
   cd /opt/time_slave
   ./bin/time_slave --config etc/time_slave_config.json

To use a non-default interface without a config file:

.. code-block:: shell

   GPTP_IFACE=emac1 ./bin/time_slave

Reference integration test
**************************

A reference integration test is provided at
``score/time_slave/tests/reference_integration/``. It boots a minimal
QNX QEMU image containing the TimeSlave binary and a QNX configuration
file, starts the process, and verifies that it configures itself
correctly, opens the shared-memory channel, and begins publishing gPTP
snapshots.

Run it with:

.. code-block:: shell

   bazel test --config=x86_64-qnx \
       //score/time_slave/tests/reference_integration:time_slave_qnx_ref_integ
