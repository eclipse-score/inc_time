/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#ifndef SCORE_TIME_SLAVE_SRC_GPTP_GPTP_ENGINE_H
#define SCORE_TIME_SLAVE_SRC_GPTP_GPTP_ENGINE_H

#include "score/time_slave/src/gptp/details/frame_codec.h"
#include "score/time_slave/src/gptp/details/message_parser.h"
#include "score/time_slave/src/gptp/details/network_identity.h"
#include "score/time_slave/src/gptp/details/pdelay_measurer.h"
#include "score/time_slave/src/gptp/details/ptp_types.h"
#include "score/time_slave/src/gptp/details/raw_socket.h"
#include "score/time_slave/src/gptp/details/sync_state_machine.h"
#include "score/time_slave/src/gptp/phc/phc_adjuster.h"
#include "score/ts_client/src/gptp_ipc_data.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace score
{
namespace ts
{
namespace details
{

/// Configuration for GptpEngine.
struct GptpEngineOptions
{
    std::string iface_name = "emac0";                       ///< Network interface for gPTP
    int pdelay_req_interval_ms = 1000;                      ///< Period between Pdelay_Req transmissions (ms)
    int pdelay_warmup_ms = 2000;                            ///< Delay before first Pdelay_Req (ms)
    int sync_timeout_ms = 3300;                             ///< Declare timeout after this many ms without Sync
    std::int64_t jump_future_threshold_ns = 500'000'000LL;  ///< 500 ms
    PhcConfig phc_config{};                                 ///< PHC hardware clock adjustment (disabled by default)
    std::uint8_t domain_number = 0U;                        ///< gPTP domain number (0–127)
};

/**
 * @brief gPTP engine for the TimeSlave process.
 *
 * Runs two POSIX threads: RxThread (receive/parse PTP frames) and
 * PdelayThread (periodic Pdelay_Req transmission).
 *
 * Dual-snapshot design:
 *  - pending_snapshot_: filled by the RxThread on every Sync+FollowUp
 *  - current_snapshot_: a committed, fully-flagged snapshot
 *
 * Callers should:
 *  1. Call FinalizeSnapshot() to check timeout and commit pending to current.
 *  2. Call ReadPTPSnapshot() (const) to retrieve the current snapshot.
 */
class GptpEngine final
{
  public:
    explicit GptpEngine(GptpEngineOptions opts) noexcept;

    /// Constructor for testing: inject fake socket and identity.
    GptpEngine(GptpEngineOptions opts,
               std::unique_ptr<RawSocket> socket,
               std::unique_ptr<NetworkIdentity> identity) noexcept;

    ~GptpEngine() noexcept;

    GptpEngine(const GptpEngine&) = delete;
    GptpEngine& operator=(const GptpEngine&) = delete;
    GptpEngine(GptpEngine&&) = delete;
    GptpEngine& operator=(GptpEngine&&) = delete;

    /// Open the raw socket, enable HW timestamping, resolve the ClockIdentity,
    /// and start the Rx and Pdelay background threads.
    /// @return true on success.
    bool Initialize();

    /// Stop background threads and close the socket.
    /// @return true (always succeeds).
    bool Deinitialize();

    /// Check for sync timeout, apply status flags, and commit pending_snapshot_
    /// to current_snapshot_.  Must be called periodically before ReadPTPSnapshot().
    void FinalizeSnapshot() noexcept;

    /// Copy the latest committed snapshot into @p data.
    /// Non-blocking; returns false only if the engine is not initialized.
    bool ReadPTPSnapshot(score::ts::GptpIpcData& data) const noexcept;

  private:
    void RxLoop() noexcept;
    void PdelayLoop() noexcept;

    void HandlePacket(const std::uint8_t* frame, int len, const ::timespec& hwts) noexcept;
    void UpdateSnapshot(const SyncResult& sync, const PDelayResult& pdelay) noexcept;
    void SendPDelayResponseAndFollowUp(const PTPMessage& req, TmvT t2) noexcept;

    GptpEngineOptions opts_;

    std::unique_ptr<RawSocket> socket_;
    std::unique_ptr<NetworkIdentity> identity_;
    FrameCodec codec_;
    GptpMessageParser parser_;
    SyncStateMachine sync_sm_;
    std::unique_ptr<PeerDelayMeasurer> pdelay_;
    PhcAdjuster phc_;

    mutable std::mutex snapshot_mutex_;
    score::ts::GptpIpcData pending_snapshot_{};  ///< Filled by RxThread on Sync+FollowUp
    score::ts::GptpIpcData current_snapshot_{};  ///< Committed by FinalizeSnapshot()

    std::atomic<bool> running_{false};
    std::thread rx_thread_;
    std::thread pdelay_thread_;
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_SRC_GPTP_GPTP_ENGINE_H
