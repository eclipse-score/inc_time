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

.. _ts_client_detailed_design:

Time Sync Client Detailed Design
=================================

.. document:: Time Sync Client Detailed Design
   :id: doc__ts_client_detailed_design
   :status: draft
   :version: 1
   :safety: ASIL_B
   :security: NO
   :realizes: wp__sw_implementation
   :tags: ts_client

Description
-----------

The ``ts_client`` component provides shared memory-based IPC for gPTP time synchronization data exchange between TimeSlave and TimeDaemon processes. It implements a lock-free, single-writer/multi-reader communication channel using the seqlock protocol over POSIX shared memory.

Use Cases
~~~~~~~~~

1. Publishing time synchronization snapshots from TimeSlave to shared memory
2. Reading time synchronization snapshots from TimeDaemon
3. Lock-free concurrent access with bounded retry on torn reads

Rationale Behind Decomposition into Units
------------------------------------------

The ``ts_client`` component is decomposed into two implementation units:

1. **GptpIpcPublisher** — Creates and writes to the shared memory segment (TimeSlave side)
2. **GptpIpcReceiver** — Opens and reads from the shared memory segment (TimeDaemon side)

This separation enables independent deployment in different processes while maintaining a consistent IPC protocol.

Static Diagrams for Unit Interactions
--------------------------------------

Class View
~~~~~~~~~~

Main classes and their relationships:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ipc_channel.puml
   :alt: Class View

.. raw:: html

   </div>

Units within the Component
--------------------------

The relationship between a unit and its parent component is established implicitly
through the file path. Each component has its own directory, and units residing
within that directory belong to it. The unit's attributes and behaviour are documented
in the source code itself.

- **GptpIpcPublisher**: Creates and writes to shared memory using seqlock protocol (see ``src/gptp_ipc_publisher.h``)
- **GptpIpcReceiver**: Reads from shared memory with bounded retry on torn reads (see ``src/gptp_ipc_receiver.h``)

Seqlock Protocol Workflow
~~~~~~~~~~~~~~~~~~~~~~~~~~

The seqlock protocol ensures lock-free communication between publisher and receiver:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ipc_sequence.puml
   :alt: Seqlock Protocol

.. raw:: html

   </div>

Shared Memory Layout
~~~~~~~~~~~~~~~~~~~~

The ``GptpIpcRegion`` structure defines the shared memory layout:

.. code-block:: cpp

   struct alignas(64) GptpIpcRegion
   {
       std::atomic<std::uint32_t> magic{0x47505450};  // 'GPTP'
       std::atomic<std::uint32_t> seq{0};
       score::ts::GptpIpcData data{};
       std::atomic<std::uint32_t> seq_confirm{1};
   };

- Aligned to 64 bytes (cache line size) to prevent false sharing
- ``magic`` field validates the segment on reader initialization
- ``seq`` and ``seq_confirm`` implement the seqlock protocol
- ``data`` contains the ``GptpIpcData`` payload

Using in Test Environment
--------------------------

The ``GptpIpcPublisher`` and ``GptpIpcReceiver`` rely on POSIX shared memory (``shm_open``), which works on any Linux host. Component tests can run end-to-end using real IPC without platform-specific mocks.

Inspection Checklist
--------------------

The checklist for verification of the detailed design and code can be found here:

.. toctree::

   chklst_impl_inspection
