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

Time Slave Component Requirements
#################################

.. document:: Time Slave Requirements
   :id: doc__time_slave_requirements
   :status: valid
   :version: 1
   :safety: QM
   :security: NO
   :realizes: wp__requirements_comp[version==1]
   :tags: requirements, time_slave

Functional Requirements
-----------------------

Initialization and Lifecycle
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. comp_req:: Component Startup
   :id: comp_req__time_slave__startup
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   On startup the time_slave component shall determine its configuration, initialize the gPTP engine and IPC publisher, bind to the configured network interface, and attempt to enable hardware timestamping on the network interface during initialization. Any (unrecoverable) error shall terminate the startup procedure and end the time_slave process.

.. comp_req:: Component Configuration
   :id: comp_req__time_slave__configuration
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_ctrl_flow[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall try to read its configuration from a file. The file location is defined in this order:
    1. Specified by command line argument `--config <filepath>`,
    2. specified by environment variable `TIMESLAVE_CONFIG`,
    3. the default path `./etc/time_slave_config.json`.

   If the defined file is not present or its parsing fails (due to malformed content), a warning shall be generated and predefined default values shall be used for the configuration items.
   Likewise, if certain configuration items are missing in the file, their predefined default values shall be used.

.. comp_req:: Component Shutdown
   :id: comp_req__time_slave__shutdown
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_ctrl_flow[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   On receiving a stop signal the time_slave component shall stop message processing threads, close network sockets, and release IPC resources.

gPTP Message Processing
^^^^^^^^^^^^^^^^^^^^^^^

.. comp_req:: Domain Number Filtering
   :id: comp_req__time_slave__domain_filtering
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   According to IEEE 802.1AS, the time_slave component shall only process received gPTP Sync and FollowUp messages matching the configured domain number (0-127 per IEEE 802.1AS). The time_slave component shall only process received gPTP Pdelay messages matching the domain number 0. It shall use the domain number 0 in send gPTP Pdelay messages.

.. comp_req:: Ethernet address usage
   :id: comp_req__time_slave__ethernet_address
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall receive IEEE 802.1AS event messages (Sync, Follow_Up, Pdelay_Resp, Pdelay_Resp_Follow_Up) sent to the Ethernet address specified in the IEEE 801.1AS standard. It shall send event messages (Pdelay_Req) to the same address.

.. comp_req:: Sync Message Reception
   :id: comp_req__time_slave__sync_reception
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall receive IEEE 802.1AS Sync messages from the network Grand Master via the IEEE 802.1AS-specified Ethernet address and determine the receive timestamp using hardware timestamping when available, otherwise using software timestamping as a fallback.

.. comp_req:: Follow_Up Message Processing
   :id: comp_req__time_slave__followup_processing
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall process IEEE 802.1AS Follow_Up messages, match them to the corresponding Sync message by sequence ID, and extract the precise origin timestamp.

.. comp_req:: Offset Calculation
   :id: comp_req__time_slave__offset_calculation
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall compute the clock offset as the difference between the local receive timestamp and the Grand Master origin timestamp, accounting for the correction fields from both the Sync and Follow_Up messages per IEEE 802.1AS.

.. comp_req:: PDelayReq Transmission
   :id: comp_req__time_slave__pdelay_req
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall transmit IEEE 802.1AS PDelayReq messages at the configured interval (configurable, default 1000 milliseconds) and record the transmit timestamp.

.. comp_req:: PDelayReq Response
   :id: comp_req__time_slave__pdelay_req_response
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall receive IEEE 802.1AS PDelayReq messages from peer nodes, record the receive timestamp, and respond with PDelayResp and PDelayRespFollowUp messages containing the requestReceiptTimestamp and responseOriginTimestamp per IEEE 802.1AS peer delay mechanism.


.. comp_req:: PDelayResp Message Reception
   :id: comp_req__time_slave__pdelay_resp_reception
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall receive IEEE 802.1AS PDelayResp messages in response to transmitted PDelayReq messages and extract the requestReceiptTimestamp from the message.

.. comp_req:: PDelayRespFollowUp Message Reception
   :id: comp_req__time_slave__pdelay_resp_fu_rx
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall receive IEEE 802.1AS PDelayRespFollowUp messages, match them to the corresponding PDelayResp message by sequence ID, and extract the responseOriginTimestamp.

.. comp_req:: Peer Delay Computation
   :id: comp_req__time_slave__pdelay_computation
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall compute network propagation delay from PDelayResp and PDelayRespFollowUp timestamps using the IEEE 802.1AS propagation delay formula: ((t4 - t1) - (t3 - t2)) / 2, where t2 is the requestReceiptTimestamp and t3 is the responseOriginTimestamp each corrected by the correctionField of the corresponding PdelayResp or PdelayRespFollowUp message.

Clock Synchronization
^^^^^^^^^^^^^^^^^^^^^

.. comp_req:: PHC Offset Adjustment
   :id: comp_req__time_slave__phc_offset
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall step-adjust the PTP hardware clock (PHC - the network interface hardware clock used for timestamping per IEEE 1588/802.1AS) offset when the absolute value of the computed offset exceeds the configured step threshold (configurable, default 1 second).

.. comp_req:: PHC Frequency Adjustment
   :id: comp_req__time_slave__phc_frequency
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall slew-adjust the PTP hardware clock frequency based on the neighbor rate ratio computed from consecutive Sync/Follow_Up pairs with positive time intervals per IEEE 802.1AS Clause 11.4.1.


Data Publishing
^^^^^^^^^^^^^^^

.. comp_req:: Time Sync Data Publishing
   :id: comp_req__time_slave__sync_publishing
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall publish synchronized time data (PTP assumed time, offset correction, rate deviation, peer delay, status flags) via the ts_client IPC interface at a fixed interval of 50 milliseconds to provide consistent read latency for VehicleTime clients.

Platform Abstraction
^^^^^^^^^^^^^^^^^^^^

.. comp_req:: Platform Support
   :id: comp_req__time_slave__platform_support
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall support Linux and QNX 8.0 SDP platforms for Ethernet frame transmission/reception with hardware timestamping and PTP hardware clock control.

Error Handling
^^^^^^^^^^^^^^

.. comp_req:: Error Reporting
   :id: comp_req__time_slave__error_reporting
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync_log[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall log messages via the score::mw::log interface when initialization fails (error level), network operations fail (error level), protocol errors occur (error level), or hardware timestamping is unavailable (warning level).

Diagnostics and Debugging
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. comp_req:: Synchronization Diagnostics
   :id: comp_req__time_slave__diagnostics
   :reqtype: Functional
   :security: NO
   :safety: QM
   :derived_from: feat_req__time__vehicle_time_sync_log[version==1]
   :status: valid
   :version: 1
   :satisfied_by: comp__time_slave

   The time_slave component shall provide optional runtime instrumentation to record gPTP synchronization events (peer delay completion, offset thresholds, time leaps) to a CSV file for debugging and diagnostics when enabled via the GptpEngineOptions diagnostics configuration parameter.

Assumption of Use Requirements
------------------------------

.. aou_req:: Network Interface Configuration
   :id: aou_req__time_slave__network_config
   :reqtype: Process
   :security: NO
   :safety: QM
   :status: valid
   :version: 1

   The user shall configure a network interface and ensure the interface has an assigned link-layer address and the link is operationally up (IFF_UP and IFF_RUNNING flags set) before starting time_slave.

.. aou_req:: Single Instance per Interface
   :id: aou_req__time_slave__single_instance
   :reqtype: Process
   :security: NO
   :safety: QM
   :status: valid
   :version: 1

   The user shall run exactly one time_slave instance per network interface to prevent gPTP protocol conflicts. Multiple instances binding to the same interface will cause raw socket binding failures or multicast group membership conflicts.

.. needextend:: "c.this_doc()"
   :+tags: time_slave


.. toctree::
   :maxdepth: 1

   chklst_req_inspection
