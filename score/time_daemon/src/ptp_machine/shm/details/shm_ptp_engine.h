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
#ifndef SCORE_TIME_DAEMON_SRC_PTP_MACHINE_SHM_DETAILS_SHM_PTP_ENGINE_H
#define SCORE_TIME_DAEMON_SRC_PTP_MACHINE_SHM_DETAILS_SHM_PTP_ENGINE_H

#include "score/time_daemon/src/common/data_types/ptp_time_info.h"
#include "score/ts_client/src/gptp_ipc_receiver.h"

#include <string>

namespace score
{
namespace td
{
namespace details
{

/// @brief PTP engine implementation that reads @c GptpIpcData from the shared
/// memory channel written by TimeSlave via @c GptpIpcPublisher.
///
/// Converts the @c GptpIpcData (ts_client-internal type) to the @c PtpTimeInfo
/// structure expected by the TimeDaemon pipeline. Instantiated as
/// @c GPTPShmMachine (type alias for @c PTPMachine<details::ShmPTPEngine>),
/// connecting this engine to the TimeDaemon MessageBroker.
///
/// @see CreateGPTPShmMachine Factory function.
/// @see GptpIpcReceiver In ts_client, for the shared memory protocol.
/// @see PTPMachine Template wrapper.
class ShmPTPEngine final
{
  public:
    explicit ShmPTPEngine(std::string ipc_name = score::ts::details::kGptpIpcName) noexcept;
    ~ShmPTPEngine() noexcept = default;

    ShmPTPEngine(const ShmPTPEngine&) = delete;
    ShmPTPEngine& operator=(const ShmPTPEngine&) = delete;
    ShmPTPEngine(ShmPTPEngine&&) = delete;
    ShmPTPEngine& operator=(ShmPTPEngine&&) = delete;

    /// @brief Opens the shared memory IPC channel written by TimeSlave.
    ///
    /// Calls @c GptpIpcReceiver::Init(ipc_name_) to map the shared memory
    /// segment. Must be called once before @c ReadPTPSnapshot().
    ///
    /// @return @c true if the channel was opened successfully.
    bool Initialize();

    /// @brief Closes the shared memory IPC channel.
    ///
    /// Calls @c GptpIpcReceiver::Close() to unmap the shared memory region.
    ///
    /// @return @c true (always succeeds).
    bool Deinitialize();

    /// @brief Reads the latest gPTP snapshot from shared memory and converts it
    /// to @c PtpTimeInfo.
    ///
    /// Calls @c GptpIpcReceiver::Receive() then performs a field-by-field
    /// mapping from @c GptpIpcData to @c PtpTimeInfo:
    ///
    /// | @c GptpIpcData field            | @c PtpTimeInfo field                           |
    /// |---------------------------------|------------------------------------------------|
    /// | @c ptp_assumed_time             | @c ptp_assumed_time                            |
    /// | @c local_time                   | @c local_time (wrapped in ReferenceClock::time_point) |
    /// | @c rate_deviation               | @c rate_deviation                              |
    /// | @c status.is_synchronized       | @c status.is_synchronized                      |
    /// | @c status.is_timeout            | @c status.is_timeout                           |
    /// | @c status.is_time_jump_future   | @c status.is_time_jump_future                  |
    /// | @c status.is_time_jump_past     | @c status.is_time_jump_past                    |
    /// | @c status.is_correct            | @c status.is_correct                           |
    /// | @c sync_fup_data.* (9 fields)   | @c sync_fup_data.* (direct copy)               |
    /// | @c pdelay_data.* (12 fields)    | @c pdelay_data.* (direct copy)                 |
    ///
    /// All @c GptpIpcData fields mapped 1:1. Unmapped @c PtpTimeInfo fields zero-initialized.
    ///
    /// @param[out] info Output parameter filled with the converted snapshot.
    /// @return @c true if a valid snapshot was read; @c false otherwise.
    bool ReadPTPSnapshot(PtpTimeInfo& info);

  private:
    std::string ipc_name_;
    score::ts::details::GptpIpcReceiver receiver_;
    bool initialized_{false};
};

}  // namespace details
}  // namespace td
}  // namespace score

#endif  // SCORE_TIME_DAEMON_SRC_PTP_MACHINE_SHM_DETAILS_SHM_PTP_ENGINE_H
