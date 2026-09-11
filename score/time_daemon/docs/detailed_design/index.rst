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

.. _time_daemon_detailed_design:

Time Daemon Detailed Design
===========================

.. document:: Time Daemon Detailed Design
   :id: doc__time_daemon_detailed_design
   :status: draft
   :version: 1
   :safety: ASIL_B
   :security: YES
   :realizes: wp__sw_implementation
   :tags: time_daemon

Description
-----------

Use Cases
~~~~~~~~~

TimeDaemon is the non Autosar adaptive process who is intended to get the Vehicle Time from the ptp slave daemon (ptpd or any other), verify and validate the timepoints and distribute time information across the clients.

More precisely we can specify the following use cases for the time daemon:

1. Providing current Vehicle time to different applications
2. Setting the synchronization qualifier (aka Synchronized, Timeout, so on)
3. Providing needed information for diagnostics
4. Providing needed information for addition verification, ex SafeCarTime

The raw architectural diagram is represented below.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/sad_deployment.puml
   :alt: Raw architectural diagram

.. raw:: html

   </div>

Rationale Behind Decomposition into Units
------------------------------------------

TimeDaemon is decomposed into six implementation units following SOLID principles
(Single Responsibility, Open/Closed) and design patterns (Publish-Subscribe, State Machines):

1. **Application** — Orchestrates initialization and lifecycle of all daemon components
2. **Message Broker** — Central publish-subscribe hub for decoupled inter-unit communication
3. **ControlFlowDivider** — Separates execution threads to prevent blocking and maintain data flow consistency
4. **PTP Machine** — Retrieves raw time data from PTP stack at consistent rates
5. **Verification Machine** — Validates and qualifies time data (sync status, jump detection, timeout)
6. **IPC Machine** — Exports qualified time snapshots to client applications via shared-memory interface

This separation enables independent testing, reusable implementations (e.g., different PTP stacks),
and clear responsibility boundaries critical for ASIL_B safety qualification.

Static Diagrams for Unit Interactions
--------------------------------------

Class View
~~~~~~~~~~

Main classes and unit relationships are presented on this diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/dd_class.puml
   :alt: Class View
   :width: 100%
   :align: center

.. raw:: html

   </div>

Deployment View
~~~~~~~~~~~~~~~

The design deployment and process architecture is represented on this diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/dd_deployment.puml
   :alt: Deployment View

.. raw:: html

   </div>

Dynamic Diagrams for Unit Interactions
---------------------------------------

Data and Control Flow
~~~~~~~~~~~~~~~~~~~~~

The data and control flow between units is presented in the following diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/dd_data_control_flow.puml
   :alt: Data and Control flow View

.. raw:: html

   </div>

On this view you can see several execution scopes:

1. **PTP retrieving scope** — retrieves latest PTP data and publishes to ``raw_ptp_data`` topic
2. **PTPTimeInfo handling scope** — validates time data and publishes to ``verified_ptp_data`` topic
3. **PTPTimeInfo receiving scope** — propagates qualified time to client applications

Each control flow is implemented with a dedicated thread or process and is independent from the others.

Data Types or Events
^^^^^^^^^^^^^^^^^^^^^

Main data exchanged between units via MessageBroker topics:

- ``PtpTimeInfo`` — internal time snapshot struct containing sync data, peer delay, status flags, and timestamps

Topic names:

.. _raw_ptp_data:

.. rubric:: raw_ptp_data

Raw PTP snapshot from ``PtpMachine``; published to ``ControlFlowDivider`` for thread separation.

.. _input_ptp_data:

.. rubric:: input_ptp_data

Same data as :ref:`raw_ptp_data` but republished at consistent rate by ``ControlFlowDivider``; consumed by ``VerificationMachine``.

.. _verified_ptp_data:

.. rubric:: verified_ptp_data

Validated and qualified time snapshot from ``VerificationMachine``; includes sync status, timeout flags, time jump detection; published to ``PublisherImpl``.

Units within the Component
---------------------------

The following units comprise TimeDaemon's internal implementation:

- ``TimeDaemon`` — main entry point orchestrating lifecycle and initialization
- ``MessageBroker`` — publish-subscribe hub for decoupled inter-unit communication
- ``ControlFlowDivider`` — separates execution threads to prevent blocking
- ``PtpMachine`` — retrieves raw time data from PTP stack
- ``ShmPtpEngine`` — reads gPTP data from TimeSlave shared memory channel
- ``VerificationMachine`` — validates and qualifies time data
- ``PublisherImpl`` — publishes qualified time snapshots via shared memory
- ``ReceiverImpl`` — receives time data from shared memory

Per-Unit Diagrams
~~~~~~~~~~~~~~~~~

Application
^^^^^^^^^^^

The ``Application`` component is the main entry point for TimeDaemon, orchestrating lifecycle and initialization of all daemon components.

The ``TimebaseHandler`` component implements timebase-specific logic. Multiple handlers may exist per supported timebase count, allowing different timebase implementations while maintaining consistent application structure.

During initialization, the ``Application`` uses a factory pattern to create components in specific order:

- Create ``MessageBroker`` first (other components depend on it)
- Create ProactiveMachines (``PtpMachine``, ``ControlFlowDivider``) that drive system behavior, then initialize and wire subscriptions
- Create ReactiveMachines (``VerificationMachine``, ``IPCMachine``) that respond to events, then initialize and wire subscriptions

During execution, ProactiveMachines start in correct order. On termination, they stop in reverse order.

Class diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/app/app_class.puml
   :alt: Application class diagram

.. raw:: html

   </div>

Initialization sequence showing component creation order and MessageBroker subscription wiring:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/app/app_init_seq.puml
   :alt: Application initialization sequence

.. raw:: html

   </div>

Runtime workflow showing ProactiveMachine startup and shutdown coordination:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/app/app_workflow_seq.puml
   :alt: Application workflow sequence

.. raw:: html

   </div>

MessageBroker
^^^^^^^^^^^^^

Class diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/msg_broker/msg_broker_class.puml
   :alt: MessageBroker class diagram

.. raw:: html

   </div>

Initialization workflow:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/msg_broker/msg_broker_init_seq.puml
   :alt: MessageBroker initialization

.. raw:: html

   </div>

Message flow through MessageBroker showing topic distribution to multiple subscribers:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/msg_broker/msg_broker_workflow_seq.puml
   :alt: MessageBroker message distribution

.. raw:: html

   </div>

ControlFlowDivider
^^^^^^^^^^^^^^^^^^

Class diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ctrlflow/ctrlflow_class.puml
   :alt: ControlFlowDivider class diagram

.. raw:: html

   </div>

Initialization workflow:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ctrlflow/ctrlflow_init_seq.puml
   :alt: ControlFlowDivider initialization

.. raw:: html

   </div>

Thread separation and periodic republishing workflow:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ctrlflow/ctrlflow_workflow_seq.puml
   :alt: ControlFlowDivider workflow

.. raw:: html

   </div>

PTPMachine
^^^^^^^^^^

Class diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ptp_machine/ptp_machine_class.puml
   :alt: PTPMachine class diagram

.. raw:: html

   </div>

Initialization workflow:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ptp_machine/ptp_machine_init_seq.puml
   :alt: PTPMachine initialization

.. raw:: html

   </div>

PTP data retrieval cycle from shared memory:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ptp_machine/ptp_machine_get_new_data_seq.puml
   :alt: PTPMachine data retrieval

.. raw:: html

   </div>

ShmPTPEngine
^^^^^^^^^^^^

Class diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/shm_ptp_engine/shm_ptp_engine_class.puml
   :alt: ShmPTPEngine class diagram

.. raw:: html

   </div>

Initialization workflow:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/shm_ptp_engine/shm_ptp_engine_init_seq.puml
   :alt: ShmPTPEngine initialization

.. raw:: html

   </div>

Shared memory read from TimeSlave gPTP publisher:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/shm_ptp_engine/shm_ptp_engine_read_seq.puml
   :alt: ShmPTPEngine read sequence

.. raw:: html

   </div>

VerificationMachine
^^^^^^^^^^^^^^^^^^^

Class diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ver_machine/ver_class.puml
   :alt: VerificationMachine class diagram

.. raw:: html

   </div>

Initialization workflow:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ver_machine/ver_init_seq.puml
   :alt: VerificationMachine initialization

.. raw:: html

   </div>

Time data validation stages (sync check, timeout, jump detection):

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ver_machine/ver_verification_seq.puml
   :alt: VerificationMachine pipeline

.. raw:: html

   </div>

IPC Machine
^^^^^^^^^^^

Class diagram:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ipc/ipc_class.puml
   :alt: IPC class diagram

.. raw:: html

   </div>

Initialization workflow:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ipc/ipc_init_seq.puml
   :alt: IPC initialization

.. raw:: html

   </div>

Qualified time snapshot publishing to client shared memory:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ipc/ipc_publish_seq.puml
   :alt: IPC publish sequence

.. raw:: html

   </div>

Client-side receive from TimeDaemon shared memory channel:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/ipc/ipc_receive_seq.puml
   :alt: IPC receive sequence

.. raw:: html

   </div>

Logging configuration
~~~~~~~~~~~~~~~~~~~~~

The daemon should have the following logging contexts:

.. list-table:: Logging Contexts
   :header-rows: 1
   :widths: 30 20 50

   * - component
     - App/Context ID
     - Comments
   * - TimeDaemon
     - TDON
     - **T**\ ime\ **D**\ aem\ **ON**
   * - Application
     - TDAP
     - **T**\ ime\ **D**\ aemon **AP**\ plication
   * - MessageBroker
     - TDMB
     - **T**\ ime\ **D**\ aemon **M**\ essage\ **B**\ roker
   * - ControlFlowDivider
     - TDCD
     - **T**\ ime\ **D**\ aemon **C**\ ontrolFlow\ **D**\ ivider
   * - PTPMachine
     - TDPM
     - **T**\ ime\ **D**\ aemon **P**\ TP\ **M**\ achine
   * - ShmPTPEngine
     - GPTP
     - **GPTP** Shm adapter (Initialize / ReadPTPSnapshot)
   * - VerificationMachine
     - TDVM
     - **T**\ ime\ **D**\ aemon **V**\ erification\ **M**\ achine
   * - IPCMachine::receiver
     - TDIR
     - **T**\ ime\ **D**\ aemon **I**\ PCMachine::\ **R**\ eceiver
   * - IPCMachine::publisher
     - TDIP
     - **T**\ ime\ **D**\ aemon **I**\ PCMachine::\ **P**\ ublisher

Variability
~~~~~~~~~~~

Configuration files
^^^^^^^^^^^^^^^^^^^

The ``TimeDaemon`` uses structured configuration files to enable customization of its runtime behavior. These data could be configured:

1. Component-specific Configuration:

   a. Each component can have dedicated configuration sections
   b. Parameters such as update rates, timeouts, and thresholds can be specified

2. Topic Configuration:

   a. Topics for the ``Message Broker`` can be defined in configuration
   b. Publisher and subscriber relationships can be specified externally
   c. Component roles (publisher/subscriber) can be assigned through configuration

3. File Format and Structure: The configuration files use JSON format for readability and easy parsing:

.. code-block:: json

   {
     "message_broker": {
       "topics": [
         {
           "name": "raw_ptp_data",
           "publishers": ["PtpMachine"],
           "subscribers": ["ControlFlowDivider"]
         },
         {
           "name": "input_ptp_data",
           "publishers": ["ControlFlowDivider"],
           "subscribers": ["VerificationMachine"]
         },
         {
           "name": "verified_ptp_data",
           "publishers": ["VerificationMachine"],
           "subscribers": ["IPCMachine"]
         }
       ]
     },
     "ptp_machine": {
       "update_interval_ms": 50,
       "ptp_stack_type": "ptp",
       "ptp_stack_parameters": {
         "device": "/dev/ptp0"
       }
     },
     "control_flow_divider": {
       "timeout_ms": 500,
       "publishing_rate_ms": 100
     },
     "verification_machine": {
       "validation_stages": ["synchronization", "timejumps", "timeout"],
       "timejumps_parameters": {
         "max_backward_jump_ns": 100000
       },
       "timeout_parameters": {
         "threshold_ns": 100000
       }
     },
     "ipc_machine": {
       "shared_memory_name": "vehicle_time",
       "shared_memory_size": 4096
     }
   }

Scalability
^^^^^^^^^^^

The ``TimeDaemon``'s architecture supports scalability in the following ways:

Component Extensibility:
''''''''''''''''''''''''

1. New machine components can be added by implementing the ``BaseMachine`` interface
2. Additional validation stages can be plugged into the ``VerificationMachine`` pipeline
3. Alternative IPC mechanisms or communication with ptp stack can be implemented by alternative the ``IPCMachine`` or ``PTPMachine`` implementation

Example based on Qualified Vehicle Time integration
'''''''''''''''''''''''''''''''''''''''''''''''''''

The ``Qualified Vehicle Time`` integration extends the standard ``TimeDaemon`` architecture with:

1. A ``Qualified Vehicle Time`` component that performs additional time qualification and provide new topics: ``qualified_ptp_data`` and ``diagnostic_sct_data``
2. A dedicated IPC channel for SCT diagnostic data
3. A ``score::time::qvt`` library for diagnostic applications

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/examples/qvt/qvt_deployment.puml
   :alt: Deployment view

.. raw:: html

   </div>

The ``Qualified Vehicle Time`` component is integrated into the existing processing pipeline:

1. It subscribes to the :ref:`verified_ptp_data` topic from the ``VerificationMachine``
2. It processes and qualifies the time data with additional QVT-specific checks
3. It publishes two types of data:

   a. Qualified time data to the standard IPC Machine towards clients interested in the qualified Vehicle Time
   b. Diagnostic data to a dedicated QVT IPC channel towards Diagnostic and Central Validator notifications

The extended data flow with Qualified Vehicle Time integration is shown below:

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/examples/qvt/qvt_data_control_flow.puml
   :alt: Data flow

.. raw:: html

   </div>

Example based on Absolute Time integration
''''''''''''''''''''''''''''''''''''''''''

Another example of the ``TimeDaemon`` extension is the integration of an ``Absolute Time`` source, such as GNSS, to provide absolute time information alongside the relative Vehicle Time from PTP.

The ``Absolute Time`` integration extends the standard ``TimeDaemon`` architecture with:

1. An ``SDatMachine`` component that retrieves absolute time from GNSS via SOMEIP or other sources and provide new topics: ``absolute_time_data``
2. A dedicated verification stage in the ``VerificationMachine`` for Absolute Time qualification
3. A dedicated IPC channel for Absolute Time data
4. A ``score::time::abs`` library for applications requiring absolute time on Clients side.

The way how it is integrated is presented below.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/examples/abs_time/abs_time_deployment.puml
   :alt: Data flow

.. raw:: html

   </div>


The control and data flow with Absolute Time integration is shown below.

.. raw:: html

   <div style="overflow-x: auto; max-width: 100%;">

.. uml:: _assets/examples/abs_time/abs_time_data_control_flow.puml
   :alt: Data flow

.. raw:: html

   </div>

Using in test environment
~~~~~~~~~~~~~~~~~~~~~~~~~~

Using in ITF
^^^^^^^^^^^^

Normal behavior is expected.

Using in Component Tests on the host
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Overview
''''''''

The ``TimeDaemon`` can be utilized in the ``Component Tests`` environment to enable comprehensive testing of time-dependent components without relying on physical PTP hardware.
This approach allows test cases to manipulate time values and synchronization states to validate application behavior under various timing conditions.

For the Component tests the ``PtpMachine::PtpEngine`` library is the only one platform-dependent.
Thus the ``TimeDaemon`` components remain largely unchanged except for the ``PTPMachine`` component, which is replaced with an test-specific implementation that can be controlled via test cases
This component shall:

1. simulate "normal" ``PTPMachine`` behavior
2. have the communication channel to the test case and react on the manipulations

Next steps: plugin system
~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``TimeDaemon`` could be extended with a flexible plugin system that enables dynamic component loading, configuration, subscription and extension without requiring code changes or recompilation.

Plugin Architecture
^^^^^^^^^^^^^^^^^^^

The plugin system is structured around the following key elements:

1. ``Component Registry``: A central registry that maintains information about available component implementations
2. ``Component Factory``: Creates component instances based on configuration
3. ``Plugin Manager``: Loads and initializes plugins at runtime
4. ``Configuration-Driven Assembly``: Components and their relationships defined in configuration files

Component Creation Process
^^^^^^^^^^^^^^^^^^^^^^^^^^^

During ``TimeDaemon`` initialization:

1. The ``Plugin Manager`` loads all specified plugins from configured directories or bazel targets
2. Each plugin registers its component factories with the registry
3. The ``Application`` reads the component configuration
4. For each component in the configuration:

   a. The appropriate factory is retrieved from the registry
   b. The component is created with its specified parameters
   c. Components are connected based on the ``MessageBroker`` topic configuration

ASIL-B qualification
~~~~~~~~~~~~~~~~~~~~~

Clean separation of concerns allows the ``VehicleClock`` ``td_impl`` backend as well as ``TimeDaemon`` to be qualified according to ASIL-B requirements following ISO 26262 standard.

Inspection Checklist
---------------------

The checklist for verification of the detailed design and code can be found here:

.. toctree::

   chklst_impl_inspection
