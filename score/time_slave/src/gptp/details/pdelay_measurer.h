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
#ifndef SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_PDELAY_MEASURER_H
#define SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_PDELAY_MEASURER_H

#include "score/time_slave/src/gptp/details/ptp_types.h"
#include "score/time_slave/src/gptp/details/raw_socket.h"
#include "score/ts_client/src/gptp_ipc_data.h"

#include <cstdint>
#include <mutex>

namespace score
{
namespace ts
{
namespace details
{

/// Result produced by a completed Pdelay measurement cycle.
struct PDelayResult
{
    std::int64_t path_delay_ns{0};
    score::ts::GptpIpcPDelayData pdelay_data{};
    bool valid{false};
};

/// @brief Measures the one-way peer delay using the IEEE 802.1AS Pdelay mechanism.
///
/// Implements the IEEE 802.1AS two-step peer-delay measurement. The formula
/// applied by @c OnResponseFollowUp() when a complete cycle is received is:
///
///   path_delay = ((t2 − t1) + (t4 − t3c)) / 2
///
/// Thread-safety: @c SendRequest() is called from the PdelayThread.
///               @c OnResponse(), @c OnResponseFollowUp(), and @c GetResult()
///               are called from the RxThread. Mutex protects: @c seqnum_,
///               @c resp_count_, @c req_, @c resp_, @c resp_fup_, @c result_.
class PeerDelayMeasurer final
{
  public:
    /// @brief Construct a new PeerDelayMeasurer with the local ClockIdentity and gPTP domain.
    ///
    /// @param local_identity  The local ClockIdentity (used in Pdelay_Req).
    /// @param domain          The gPTP domain number (0–127).
    explicit PeerDelayMeasurer(const ClockIdentity& local_identity, std::uint8_t domain = 0U) noexcept;

    /// @brief Build and transmit a Pdelay_Req frame. @p socket must be open.
    ///
    /// Increments the sequence number, encodes the frame, transmits it via @c socket, and records @c t1 (transmit
    /// hardware timestamp; used in path_delay formula when FollowUp arrives).
    ///
    /// @return 0 on success, negative on error.
    int SendRequest(RawSocket& socket);

    /// @brief Process an incoming Pdelay_Resp message.
    ///
    /// Called from the RxThread. Records @c t2 (the responder's receive
    /// hardware timestamp) and the responder's identity for the current sequence.
    void OnResponse(const PTPMessage& msg);

    /// @brief Process an incoming Pdelay_Resp_Follow_Up message; triggers path delay computation.
    ///
    /// Called from the RxThread. Records @c t3c (the responder's transmit
    /// correction) and computes the path delay using the IEEE 802.1AS formula
    /// if @c t1, @c t2, and @c t4 are all available.
    void OnResponseFollowUp(const PTPMessage& msg);

    /// @brief Returns the latest computed measurement (or invalid if none yet available).
    PDelayResult GetResult() const;

  private:
    void ComputeAndStoreUnlocked() noexcept;

    ClockIdentity local_identity_{};
    std::uint8_t domain_{0U};

    mutable std::mutex mutex_;

    std::uint16_t seqnum_{0U};
    std::uint16_t resp_count_{0U};  // Pdelay_Resp messages received for the current request
    PTPMessage req_{};
    PTPMessage resp_{};
    PTPMessage resp_fup_{};
    PDelayResult result_{};
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_PDELAY_MEASURER_H
