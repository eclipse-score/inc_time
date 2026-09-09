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

/// @brief All configurable parameters for the gPTP engine.
///
/// Platform-specific defaults apply for QNX (@c emac0) and Linux (@c /dev/ptp0).
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

/// @brief Core gPTP protocol engine for the TimeSlave process.
///
/// Manages two background threads for network I/O and peer delay measurement,
/// exposing thread-safe @c ReadPTPSnapshot() for the main thread.
///
/// @b RxThread responsibilities:
///   1. Receive raw gPTP Ethernet frames with hardware timestamps from the NIC via raw sockets.
///   2. Decode and parse PTP messages (Sync, FollowUp, PdelayResp, PdelayRespFollowUp).
///   3. Correlate Sync/FollowUp pairs and compute the clock offset and neighborRateRatio.
///   4. Update @c pending_snapshot_ under @c snapshot_mutex_ protection.
///
/// @b PdelayThread responsibilities:
///   1. Periodically transmit PDelayReq frames and capture hardware transmit timestamps.
///   2. Coordinate with RxThread to receive PDelayResp and PDelayRespFollowUp messages.
///   3. Compute peer delay using the IEEE 802.1AS formula:
///      @c path_delay = ((t2 \u2212 t1) + (t4 \u2212 t3c)) / 2
///
/// @b Dual-snapshot design:
///   - @c pending_snapshot_: filled by the RxThread on every Sync+FollowUp.
///   - @c current_snapshot_: committed snapshot, advanced by @c FinalizeSnapshot().
///
/// @c FinalizeSnapshot() must be called regularly (typically main loop); missing calls delay commits.
///
/// @c PeerDelayMeasurer uses its own @c std::mutex for PdelayThread / RxThread
/// synchronisation. @c SyncStateMachine uses @c std::atomic for its timeout flag.
///
/// A test constructor accepts injected @c RawSocket and @c NetworkIdentity
/// dependencies for white-box unit testing without PTP hardware.
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

    /// @brief Opens the raw socket, enables hardware timestamping, resolves the
    /// @c ClockIdentity, and starts the Rx and PDelay background threads.
    ///
    /// Calls @c RawSocket::EnableHwTimestamping() to request NIC-level receive
    /// timestamps (@c SO_TIMESTAMPING on Linux). If the NIC does not support
    /// hardware timestamping, the call returns @c false and a warning is logged;
    /// the engine continues normally with software timestamps (higher jitter but
    /// protocol correctness is unaffected).
    ///
    /// @return true on success.
    bool Initialize();

    /// @brief Stops background threads and closes the socket.
    ///
    /// @return true (always succeeds).
    bool Deinitialize();

    /// @brief Checks the sync timeout, applies status flags, and commits
    /// @c pending_snapshot_ to @c current_snapshot_ under @c snapshot_mutex_.
    ///
    /// Must be called periodically from the main thread before @c ReadPTPSnapshot().
    /// Atomic copy under @c snapshot_mutex_.
    void FinalizeSnapshot() noexcept;

    /// @brief Copies the latest committed snapshot into @p data under @c snapshot_mutex_.
    ///
    /// Non-blocking and thread-safe. Returns @c false only if the engine is not
    /// initialised.
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
