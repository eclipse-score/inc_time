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
#ifndef SCORE_TS_CLIENT_SRC_GPTP_IPC_RECEIVER_H
#define SCORE_TS_CLIENT_SRC_GPTP_IPC_RECEIVER_H

#include "score/memory/shared/shared_memory_factory.h"
#include "score/ts_client/src/gptp_ipc_channel.h"

#include <memory>
#include <optional>
#include <string>

namespace score
{
namespace ts
{
namespace details
{

/// @brief Multi-reader receiver for the gPTP IPC shared memory channel.
///
/// Opens an existing POSIX shared memory segment read-only and reads
/// @c GptpIpcData using the seqlock protocol with bounded retry on torn reads.
/// Used by @c ShmPTPEngine in TimeDaemon.
///
/// @see GptpIpcPublisher Single-writer counterpart.
class GptpIpcReceiver final
{
  public:
    /// @brief Constructs unopened receiver instance.
    GptpIpcReceiver() = default;

    /// @brief Ensures mapping teardown via @c Close().
    ~GptpIpcReceiver();

    GptpIpcReceiver(const GptpIpcReceiver&) = delete;
    GptpIpcReceiver& operator=(const GptpIpcReceiver&) = delete;

    /// @brief Opens the existing POSIX shared memory segment read-only.
    ///
    /// Opens via @c shm_open() with @c O_RDONLY, maps as read-only, and
    /// validates the magic number (@c 0x47505450).
    ///
    /// @param ipc_name Shared memory name. Default: @c kGptpIpcName (@c /gptp_ptp_info).
    /// @return true if successful.
    bool Open(const std::string& ipc_name = kGptpIpcName);

    /// @brief Reads @c GptpIpcData using the seqlock read protocol (up to 20 retries).
    ///
    /// Read sequence per attempt:
    ///   1. Read @c seq1 (acquire — must be even; odd means write in progress, skip).
    ///   2. @c memcpy of payload.
    ///   3. Acquire-release fence.
    ///   4. Read @c seq_confirm as @c seq2, re-read @c seq as @c seq3.
    ///   5. Accept only if @c seq1 == @c seq2 == @c seq3; otherwise retry.
    ///
    /// @return @c GptpIpcData if a consistent read was achieved within the retry
    ///         budget, @c std::nullopt if all retries detected a torn read.
    std::optional<score::ts::GptpIpcData> Receive();

    /// @brief Unmaps the shared memory region.
    void Close();

  private:
    const GptpIpcRegion* region_{nullptr};
    std::shared_ptr<score::memory::shared::ISharedMemoryResource> shm_resource_;
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TS_CLIENT_SRC_GPTP_IPC_RECEIVER_H
