/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_TS_CLIENT_SRC_GPTP_IPC_DATA_H
#define SCORE_TS_CLIENT_SRC_GPTP_IPC_DATA_H

#include <chrono>
#include <cstdint>

namespace score
{
namespace ts
{

/// @brief IPC-layer status flags transmitted from TimeSlave to TimeDaemon.
struct GptpIpcStatus
{
    bool is_synchronized;
    bool is_timeout;
    bool is_time_jump_future;
    bool is_time_jump_past;
    bool is_correct;
};

/// @brief Sync/FollowUp snapshot from latest received gPTP sync cycle.
struct GptpIpcSyncFupData
{
    std::uint64_t precise_origin_timestamp;
    std::uint64_t reference_global_timestamp;
    std::uint64_t reference_local_timestamp;
    std::uint64_t sync_ingress_timestamp;
    std::uint64_t correction_field;
    std::uint16_t sequence_id;
    std::uint64_t pdelay;
    std::uint32_t port_number;
    std::uint64_t clock_identity;
};

/// @brief Peer-delay measurement snapshot from latest completed pDelay cycle.
struct GptpIpcPDelayData
{
    std::uint64_t request_origin_timestamp;
    std::uint64_t request_receipt_timestamp;
    std::uint64_t response_origin_timestamp;
    std::uint64_t response_receipt_timestamp;
    std::uint64_t reference_global_timestamp;
    std::uint64_t reference_local_timestamp;
    std::uint16_t sequence_id;
    std::uint64_t pdelay;
    std::uint32_t req_port_number;
    std::uint64_t req_clock_identity;
    std::uint32_t resp_port_number;
    std::uint64_t resp_clock_identity;
};

/// @brief IPC data snapshot written by TimeSlave and read by TimeDaemon.
///
/// This type is internal to ts_client and intentionally decoupled from
/// score::td::PtpTimeInfo.  Callers are responsible for mapping between the two.
struct GptpIpcData
{
    std::chrono::nanoseconds ptp_assumed_time;
    std::chrono::nanoseconds local_time;  ///< Local monotonic time of the last Sync frame
    double rate_deviation;
    GptpIpcStatus status;
    GptpIpcSyncFupData sync_fup_data;
    GptpIpcPDelayData pdelay_data;
};

}  // namespace ts
}  // namespace score

#endif  // SCORE_TS_CLIENT_SRC_GPTP_IPC_DATA_H
