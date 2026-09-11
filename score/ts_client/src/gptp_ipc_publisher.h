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
#ifndef SCORE_TS_CLIENT_SRC_GPTP_IPC_PUBLISHER_H
#define SCORE_TS_CLIENT_SRC_GPTP_IPC_PUBLISHER_H

#include "score/memory/shared/shared_memory_factory.h"
#include "score/ts_client/src/gptp_ipc_channel.h"

#include <memory>
#include <string>

namespace score
{
namespace ts
{
namespace details
{

/// @brief Single-writer publisher for the gPTP IPC shared memory channel.
///
/// Creates the POSIX shared memory segment and writes @c GptpIpcData using
/// the seqlock protocol. Used by TimeSlave to publish time synchronisation
/// snapshots to applications (consumed by @c ShmPTPEngine / @c GptpIpcReceiver
/// in TimeDaemon).
///
/// @see @c GptpIpcReceiver Multi-reader counterpart.
class GptpIpcPublisher final
{
  public:
    /// @brief Constructs unopened publisher instance.
    GptpIpcPublisher() = default;

    /// @brief Ensures shared memory cleanup via @c Close().
    ~GptpIpcPublisher();

    GptpIpcPublisher(const GptpIpcPublisher&) = delete;
    GptpIpcPublisher& operator=(const GptpIpcPublisher&) = delete;

    /// @brief Creates and maps the POSIX shared memory segment.
    ///
    /// Opens the segment via @c shm_open() with @c O_CREAT, maps it as a
    /// @c GptpIpcRegion, and initialises the magic number to @c 0x47505450.
    ///
    /// @param ipc_name Shared memory name. Default: @c kGptpIpcName (@c /gptp_ptp_info).
    /// @return true if successful.
    bool Open(const std::string& ipc_name = kGptpIpcName);

    /// @brief Writes @c GptpIpcData using the 5-step seqlock write protocol.
    ///
    ///   1. @c seq++ (becomes odd — signals write in progress to readers).
    ///   2. Release memory fence.
    ///   3. @c memcpy of payload.
    ///   4. @c seq_confirm = seq + 1.
    ///   5. @c seq++ (both counters even — write complete).
    ///
    /// Dual seq counters detect torn reads: reader checks seq1 == seq2 == seq3
    /// after memcpy proves no concurrent write.
    ///
    /// @param data The @c GptpIpcData snapshot to publish.
    void Publish(const score::ts::GptpIpcData& data);

    /// @brief Unmaps and unlinks the shared memory segment.
    void Close();

  private:
    GptpIpcRegion* region_{nullptr};
    std::shared_ptr<score::memory::shared::ISharedMemoryResource> shm_resource_;
    std::string ipc_name_;
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TS_CLIENT_SRC_GPTP_IPC_PUBLISHER_H
