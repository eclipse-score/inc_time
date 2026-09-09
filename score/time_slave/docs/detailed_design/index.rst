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

.. _time_slave_detailed_design_index:

Time Slave Detailed Design
==========================

.. document:: Time Slave Detailed Design
   :id: doc__time_slave_detailed_design
   :status: draft
   :version: 1
   :safety: ASIL_B
   :security: NO
   :realizes: wp__sw_implementation
   :tags: time_slave

Description
-----------

Use Cases
~~~~~~~~~

TimeSlave is a standalone gPTP (IEEE 802.1AS) slave endpoint process that implements the low-level time synchronization protocol for the Eclipse SCORE time system. It is deployed as a separate process from the TimeDaemon to isolate real-time network I/O from the higher-level time validation and distribution logic.

More precisely we can specify the following use cases for the TimeSlave:

1. Receiving gPTP Sync/FollowUp messages from a Time Master on the Ethernet network
2. Measuring peer delay via the IEEE 802.1AS PDelayReq/PDelayResp exchange
3. Optionally adjusting the PTP Hardware Clock (PHC) on the NIC
4. Publishing the resulting ``GptpIpcData`` to shared memory for consumption by the TimeDaemon

The raw architectural diagram is represented below.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/timeslave_deployment.puml
   :alt: Raw architectural diagram

.. raw:: html

   </div>

Rationale Behind Decomposition into Units
-----------------------------------------

TimeSlave is decomposed into seven implementation units following SOLID principles
(Single Responsibility, Open/Closed) and design patterns (Strategy, Facade):

1. **TimeSlave Application** — Orchestrates the overall process lifecycle and periodic publish loop
2. **GptpEngine** — Core gPTP protocol engine managing the RxThread and PdelayThread for network I/O
3. **FrameCodec** — Encodes and decodes raw Ethernet frames for gPTP communication
4. **MessageParser** — Parses the PTP wire format (IEEE 1588-v2) from raw payload bytes
5. **SyncStateMachine** — Correlates Sync/FollowUp messages and computes clock offset and rate ratio
6. **PeerDelayMeasurer** — Implements the IEEE 802.1AS peer delay measurement protocol
7. **PhcAdjuster** — Synchronizes the PTP Hardware Clock on the NIC

This separation enables independent testing, exchangeability of platform-specific implementations
(raw sockets, PHC drivers), and clear responsibility boundaries critical for ASIL_B safety qualification.

TimeSlave publishes its ``GptpIpcData`` snapshot to shared memory using the ``GptpIpcPublisher``
from the :ref:`ts_client <ts_client_detailed_design>` component; the TimeDaemon-side
consumer (``ShmPTPEngine``) is documented in the :ref:`Time Daemon detailed design
<time_daemon_detailed_design>`.

Static Diagrams for Unit Interactions
-------------------------------------

Class View
~~~~~~~~~~

Main classes and unit relationships are presented on this diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/timeslave_class.puml
   :alt: Class View
   :width: 100%
   :align: center

.. raw:: html

   </div>

Dynamic Diagrams for Unit Interactions
--------------------------------------

Data and Control Flow
~~~~~~~~~~~~~~~~~~~~~

The data and control flow between units is presented in the following diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/timeslave_data_flow.puml
   :alt: Data and Control flow View

.. raw:: html

   </div>

On this view you could see several "workers" scopes:

1. RxThread scope — receive raw gPTP Ethernet frames, decode PTP messages, correlate Sync/FollowUp and handle Pdelay messages
2. PdelayThread scope — periodically transmit PDelayReq frames
3. Main thread scope — periodically publish aggregated snapshot to shared memory

See ``gptp_engine.h`` for detailed threading model, control flow responsibilities, and concurrency aspects.

Control Flows
^^^^^^^^^^^^^

Each control flow has dedicated thread and runs independently.

- **RxThread scope**

  #. receive raw gPTP Ethernet frames with hardware timestamps from NIC via raw sockets
  #. decode and parse PTP messages (Sync, FollowUp, PdelayResp, PdelayRespFollowUp, PdelayReq)
  #. depending on message type:
    #. correlate Sync/FollowUp pairs and compute clock offset and neighborRateRatio. Update shared snapshot under mutex protection
    #. correlate PdelayResp/PdelayRespFollowUp pairs with sent PdelayReq using the PeerDelayMeasurer unit and compute the propagation delay as defined in the IEEE 802.1AS standard
    #. react on incoming PdelayReq by sending PdelayResp and PdelayRespFollowUp

- **PdelayThread scope**

  #. delay sending the first PdelayReq by the configured pdelay_warmup timespan
  #. periodically trigger the PeerDelayMeasurer unit to send PdelayReq frames and capture hardware transmit timestamps

- **Main thread (periodic publish) scope**

  #. call ``GptpEngine::FinalizeSnapshot()`` to check timeout and commit pending snapshot
  #. call ``GptpEngine::ReadPTPSnapshot(data)`` to copy latest ``GptpIpcData`` to local variable
  #. publish snapshot via ``GptpIpcPublisher::Publish(data)``

Data Types or Events
^^^^^^^^^^^^^^^^^^^^

Main data exchanged between units:

- **PTPMessage** — union-based container for decoded gPTP messages plus hardware receive timestamp; produced by ``MessageParser`` and consumed by ``SyncStateMachine`` and ``PeerDelayMeasurer``
- **SyncResult** — produced by ``SyncStateMachine::OnFollowUp()``; includes computed master timestamp, clock offset, Sync/FollowUp data, and time-jump flags
- **PDelayResult** — produced by ``PeerDelayMeasurer``; includes computed path delay in nanoseconds and validity flag
- **PtpTimeInfo** — TimeDaemon-internal aggregated snapshot, not shared-memory type; produced by ``ShmPTPEngine::ReadPTPSnapshot()`` by mapping from ``GptpIpcData``

Units Within Time Slave
-----------------------

GptpEngine
~~~~~~~~~~

The ``GptpEngine`` runs RxThread and PdelayThread, and provides ``FinalizeSnapshot()`` + ``ReadPTPSnapshot()`` for periodic publish logic.

Class View
^^^^^^^^^^

The Class Diagram is presented below:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/gptp_engine/gptp_engine_class.puml
   :alt: Class Diagram

.. raw:: html

   </div>

Threading Model
^^^^^^^^^^^^^^^

The GptpEngine operates with two background threads. The threading model is represented below:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/gptp_engine/gptp_threading.puml
   :alt: Threading Model

.. raw:: html

   </div>

Concurrency Aspects
^^^^^^^^^^^^^^^^^^^

- ``std::mutex`` protects ``pending_snapshot_`` and ``current_snapshot_`` (both ``GptpIpcData``): RxThread writes pending; main thread finalizes and reads current
- ``PeerDelayMeasurer`` uses internal ``std::mutex`` to synchronize ``SendRequest()`` (PdelayThread) with ``OnResponse()`` / ``OnResponseFollowUp()`` (RxThread)
- ``SyncStateMachine`` uses ``std::atomic<bool>`` timeout flag written by RxThread and read by main thread

Hardware Timestamping Fallback
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

During ``Initialize()``, ``GptpEngine`` calls ``RawSocket::EnableHwTimestamping()`` to request NIC-level receive timestamps (``SO_TIMESTAMPING`` on Linux). If the NIC does not support hardware timestamping, the call returns ``false`` and a warning is logged:

.. code-block:: none

   GptpEngine: HW timestamping not available on <iface>, falling back to SW timestamps

The engine continues to run normally. The difference between the two modes:

.. list-table::
   :header-rows: 1
   :widths: 30 35 35

   * - Field
     - HW timestamping available
     - SW timestamping fallback
   * - ``recvHardwareTS`` (Sync receive time)
     - NIC hardware timestamp (nanosecond precision, captured at wire level)
     - Software timestamp (captured at socket receive, higher jitter)
   * - ``sync_fup_data.reference_local_timestamp``
     - Derived from NIC hardware timestamp
     - Derived from software timestamp
   * - ``GptpIpcData.local_time``
     - Always ``CLOCK_MONOTONIC`` (unaffected)
     - Always ``CLOCK_MONOTONIC`` (unaffected)
   * - Clock offset accuracy
     - High (sub-microsecond typical)
     - Reduced (jitter depends on OS scheduling latency)

The fallback does not affect protocol correctness -- Sync/FollowUp correlation and peer delay measurement continue to work -- but the computed clock offset will be less accurate due to higher receive timestamp jitter.

PeerDelayMeasurer
~~~~~~~~~~~~~~~~~~~~~~~

The ``PeerDelayMeasurer`` unit implements the IEEE 802.1AS two-step peer delay measurement protocol. It manages the four timestamps (``t1``, ``t2``, ``t3c``, ``t4``) across two threads.

Timestamp Definitions
^^^^^^^^^^^^^^^^^^^^^

.. list-table:: Peer Delay Timestamps (IEEE 802.1AS)
   :header-rows: 1
   :widths: 10 20 30 40

   * - Symbol
     - Message
     - Captured by
     - Meaning
   * - ``t1``
     - PDelayReq (TX)
     - Slave (PdelayThread)
     - HW transmit timestamp of the PDelayReq frame leaving the slave NIC
   * - ``t2``
     - PDelayResp (RX)
     - Master -> carried in PDelayResp body
     - HW receive timestamp of the PDelayReq frame arriving at the master NIC
   * - ``t3c``
     - PDelayRespFollowUp
     - Master -> carried in PDelayRespFollowUp body
     - HW transmit timestamp of the PDelayResp frame leaving the master NIC ("corrected" because it includes the master's turnaround correction)
   * - ``t4``
     - PDelayResp (RX)
     - Slave (RxThread)
     - HW receive timestamp of the PDelayResp frame arriving at the slave NIC

The peer delay formula is: ``path_delay = ((t2 - t1) + (t4 - t3c)) / 2``

- ``(t2 - t1)`` = propagation time from slave -> master
- ``(t4 - t3c)`` = propagation time from master -> slave
- The average of the two gives the one-way link delay

PhcAdjuster
~~~~~~~~~~~

The ``PhcAdjuster`` unit synchronizes the PTP Hardware Clock (PHC) on the NIC. It applies step corrections for large offsets and frequency slew for smooth convergence of small offsets.

Fallback Behavior When PHC Is Unavailable
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``PhcAdjuster`` degrades gracefully in two scenarios:

1. **PHC disabled** (``PhcConfig.enabled = false``, the default): ``AdjustOffset()`` and ``AdjustFrequency()`` are no-ops. The gPTP protocol pipeline (Sync/FollowUp reception, peer-delay measurement, ``GptpIpcData`` publishing) is completely unaffected. The hardware clock is not touched.

2. **PHC enabled but device inaccessible** (e.g., ``/dev/ptp0`` does not exist on Linux, or the EMAC interface name is wrong on QNX):

   - **Linux**: the constructor calls ``open(device, O_RDWR)``; on failure ``phc_fd_`` stays at ``-1``. Both ``AdjustOffset()`` and ``AdjustFrequency()`` guard against ``phc_fd_ < 0`` and return immediately -- a true silent skip with no system call.

   - **QNX**: ``qnx_phc_open()`` always returns ``0`` and never fails -- it only stores the device name in a thread-local context. There is no ``phc_fd_ < 0`` guard. The adjustment methods always call ``qnx_phc_adjtime_step()`` / ``qnx_phc_adjfreq_ppb()``, which internally create a UDP socket and issue ``SIOCGDRVSPEC`` / ``SIOCSDRVSPEC`` ioctls. If the socket or ioctl fails (e.g., wrong interface name, unsupported hardware), the function returns ``-1``, but the caller discards it with a ``(void)`` cast. There is no explicit skip -- the call is always attempted and errors are silently absorbed.

In both scenarios TimeSlave continues to track the master clock and publish accurate ``GptpIpcData`` snapshots (including offset and status flags) to shared memory. The downstream TimeDaemon and any applications consuming time are unaffected -- only the NIC hardware clock itself will drift relative to PTP time.

Platform Support
~~~~~~~~~~~~~~~~

TimeSlave supports two target platforms with platform-specific implementations selected at compile time via Bazel ``select()``. The ``RawSocket`` and ``NetworkIdentity`` interfaces provide the abstraction boundary.

See ``gptp_engine.h`` and ``raw_socket.h`` for hardware timestamping mechanisms (``AF_PACKET``/BPF), ``phc_adjuster.h`` for PHC adjustment APIs (``clock_adjtime`` vs QNX ioctls), and ``network_identity.h`` for MAC address retrieval methods.

Platform-specific source files are organized under ``score/time_slave/src/gptp/platform/linux/`` and ``score/time_slave/src/gptp/platform/qnx/``.

Instrumentation
~~~~~~~~~~~~~~~

TimeSlave provides two runtime instrumentation mechanisms for development and debugging:

- **ProbeManager** — singleton that traces probe events at key processing points (packet RX, Sync/FollowUp processing, peer delay completion, PHC adjustments)
- **Recorder** — thread-safe CSV file writer that appends timestamped event rows to disk

See ``probe.h`` for ProbePoint enumeration and zero-overhead ``GPTP_PROBE()`` macro.
See ``recorder.h`` for CSV format, RecordEvent types, Recorder::Config parameters, and error-handling behavior.

Logging configuration
~~~~~~~~~~~~~~~~~~~~~

TimeSlave uses the following logging contexts:

.. list-table:: Logging Contexts
   :header-rows: 1
   :widths: 35 20 45

   * - Component
     - Context ID
     - Comments
   * - TimeSlave Application
     - TSAP
     - **T**\ ime\ **S**\ lave **App**\ lication lifecycle (Initialize / Run)
   * - gPTP Engine (RxThread / PdelayThread)
     - GTPS
     - **GPTP** **SLAVE** engine — low-level protocol processing

Variability
~~~~~~~~~~~

Configuration
^^^^^^^^^^^^^

The ``GptpEngineOptions`` struct provides all configurable parameters for the gPTP engine:

.. list-table:: GptpEngine Configuration
   :header-rows: 1
   :widths: 30 15 55

   * - Parameter
     - Type
     - Description
   * - ``iface_name``
     - string
     - Network interface for gPTP frames (e.g., ``emac0``); default: ``"emac0"``
   * - ``pdelay_interval_ms``
     - int
     - Interval between PDelayReq transmissions (ms); default: ``1000``
   * - ``pdelay_warmup_ms``
     - int
     - Delay before the first PDelayReq is sent (ms); default: ``2000``
   * - ``sync_timeout_ms``
     - int
     - Timeout for Sync message reception before declaring timeout state (ms); default: ``3300``
   * - ``jump_future_threshold_ns``
     - int64_t
     - Threshold above which a positive clock offset is flagged as a forward time jump (ns); default: ``500 000 000``
   * - ``phc_config``
     - PhcConfig
     - PHC hardware clock adjustment settings (see ``PhcConfig`` table below); disabled by default

The ``PhcConfig`` struct (embedded in ``GptpEngineOptions``) contains:

.. list-table:: PhcAdjuster Configuration
   :header-rows: 1
   :widths: 30 15 55

   * - Parameter
     - Type
     - Description
   * - ``enabled``
     - bool
     - Enable or disable PHC adjustment; default: ``false``
   * - ``device``
     - string
     - PHC device identifier: ``/dev/ptp0`` on Linux, ``emac0`` on QNX
   * - ``step_threshold_ns``
     - int64_t
     - Offset threshold above which a step correction is applied instead of frequency slew (ns); default: ``100 000 000``

Scalability
^^^^^^^^^^^

The TimeSlave architecture supports the following extensibility points:

Platform extensibility
''''''''''''''''''''''

1. New target platforms can be supported by implementing the ``RawSocket`` and ``NetworkIdentity`` interfaces under a new ``platform/<os>/`` directory and selecting the implementation via ``Bazel select()``
2. The ``PhcAdjuster`` platform implementations (``clock_adjtime`` on Linux, EMAC ioctls on QNX) can be extended for additional hardware without changing protocol logic

Protocol extensibility
''''''''''''''''''''''

1. The ``GptpEngine`` accepts injected ``RawSocket`` and ``NetworkIdentity`` dependencies, making it straightforward to test or replace individual platform abstractions
2. The shared memory IPC channel name is configurable (``GptpIpcPublisher::Init(name)``), allowing multiple gPTP instances per ECU if needed

The ``GptpIpcPublisher`` used here, and the corresponding ``GptpIpcReceiver``/``ShmPTPEngine`` on the
consuming side, are documented in the :ref:`ts_client <ts_client_detailed_design>` and
:ref:`Time Daemon <time_daemon_detailed_design>` detailed designs respectively.

Using in Test Environment
~~~~~~~~~~~~~~~~~~~~~~~~~

Using in ITF
^^^^^^^^^^^^

Normal behavior is expected. TimeSlave runs as a standalone process, communicates over real Ethernet, and writes to ``/gptp_ptp_info`` shared memory as in production.

Using in Component Tests on Host
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Overview
''''''''

The ``TimeSlave`` and its constituent components can be tested on an x86 Linux host without PTP hardware or a real network. The key platform-dependent abstractions all have test-injectable counterparts:

.. list-table:: Testable Abstractions
   :header-rows: 1
   :widths: 30 35 35

   * - Abstraction
     - Production implementation
     - Test replacement
   * - ``RawSocket``
     - ``RawSocket`` (AF_PACKET)
     - ``FakeSocket`` (push-based frame queue)
   * - ``NetworkIdentity``
     - ``NetworkIdentity`` (ioctl)
     - ``FakeIdentity`` (fixed clock identity)
   * - ``HighPrecisionLocalSteadyClock``
     - Platform clock (Linux / QNX)
     - ``FakeClock`` (returns fixed timestamp)

The ``GptpEngine`` provides a dedicated test constructor that accepts injected implementations:

.. code-block:: cpp

   GptpEngine engine(opts,
                     std::make_unique<FakeSocket>(),
                     std::make_unique<FakeIdentity>());

This allows complete white-box testing of the Sync/FollowUp correlation, peer-delay measurement, timeout detection, and time-jump flagging logic by pushing crafted PTP frames directly into the ``FakeSocket`` queue.

The ``GptpIpcPublisher`` and ``GptpIpcReceiver`` rely on POSIX shared memory (``shm_open``), which works on any Linux host, so ``ShmPTPEngine`` component tests can run end-to-end using real IPC without modification.

Inspection Checklist
--------------------

The checklist for verification of the detailed design and code can be found here:

.. toctree::

   chklst_impl_inspection
