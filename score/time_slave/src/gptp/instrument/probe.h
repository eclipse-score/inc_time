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
#ifndef SCORE_TIME_SLAVE_SRC_GPTP_INSTRUMENT_PROBE_H
#define SCORE_TIME_SLAVE_SRC_GPTP_INSTRUMENT_PROBE_H

#include "score/time_slave/src/gptp/record/recorder.h"

#include <atomic>
#include <cstdint>

namespace score
{
namespace ts
{
namespace details
{

/// @brief Measurement probe points within the gPTP pipeline.
enum class ProbePoint : std::uint8_t
{
    kRxPacketReceived = 0,   ///< Raw Ethernet frame received from socket (RxThread).
    kSyncFrameParsed = 1,    ///< Sync message successfully decoded by GptpMessageParser.
    kFollowUpProcessed = 2,  ///< FollowUp received; SyncStateMachine::OnFollowUp() returned a SyncResult.
    kOffsetComputed = 3,     ///< Final clock offset value available after Sync/FollowUp correlation.
    kPdelayReqSent = 4,      ///< PDelayReq frame transmitted by PeerDelayMeasurer.
    kPdelayCompleted = 5,    ///< Peer delay computation finished (all four timestamps collected).
    kPhcAdjusted = 6,        ///< PhcAdjuster applied a step or frequency correction.
};

/// @brief Data payload for a single probe event.
struct ProbeData
{
    std::int64_t ts_mono_ns{0};
    std::int64_t value_ns{0};
    std::uint32_t seq_id{0};
};

/// @brief Singleton manager for runtime measurement probes in the gPTP pipeline.
///
/// When enabled, records probe events at key processing points (packet RX,
/// Sync/FollowUp processing, peer delay completion, PHC adjustments) to the
/// logger and optionally to a @c Recorder for CSV output.
///
/// Provides zero overhead when disabled: @c IsEnabled() is an atomic load that
/// causes an early exit in the @c GPTP_PROBE() macro before any argument
/// evaluation.
///
/// Thread-safe: @c enabled_ and @c recorder_ use @c std::atomic; @c Trace()
/// may be called concurrently from RxThread, PdelayThread, and main thread.
///
/// @see ProbePoint Enumeration of instrumentation points.
/// @see GPTP_PROBE Convenience macro for zero-overhead instrumented calls.
class ProbeManager final
{
  public:
    static ProbeManager& Instance();

    void SetEnabled(bool enabled)
    {
        enabled_.store(enabled, std::memory_order_release);
    }
    bool IsEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }

    /// @brief Optional: link to a @c Recorder for persistent CSV probe output.
    /// Pass @c nullptr to unlink.
    void SetRecorder(Recorder* recorder)
    {
        recorder_.store(recorder, std::memory_order_release);
    }

    /// @brief Records a probe event. Thread-safe.
    ///
    /// No-op if @c IsEnabled() returns @c false (fast atomic-load check).
    /// When enabled, logs the event via the middleware logger and, if a
    /// @c Recorder is linked, forwards the event for CSV file output.
    void Trace(ProbePoint point, const ProbeData& data);

  private:
    ProbeManager() = default;
    std::atomic<bool> enabled_{false};
    std::atomic<Recorder*> recorder_{nullptr};
};

/// @brief Returns the current monotonic timestamp in nanoseconds (@c CLOCK_MONOTONIC).
std::int64_t ProbeMonoNs() noexcept;

}  // namespace details
}  // namespace ts
}  // namespace score

// Convenience macro: zero overhead when probing is disabled.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GPTP_PROBE(point, ...)                                                          \
    do                                                                                  \
    {                                                                                   \
        if (::score::ts::details::ProbeManager::Instance().IsEnabled())                 \
        {                                                                               \
            ::score::ts::details::ProbeManager::Instance().Trace(point, {__VA_ARGS__}); \
        }                                                                               \
    } while (0)

#endif  // SCORE_TIME_SLAVE_SRC_GPTP_INSTRUMENT_PROBE_H
