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
#ifndef SCORE_TIME_SLAVE_SRC_GPTP_RECORD_RECORDER_H
#define SCORE_TIME_SLAVE_SRC_GPTP_RECORD_RECORDER_H

#include <atomic>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>

namespace score
{
namespace ts
{
namespace details
{

/// @brief Event types that can be recorded by the @c Recorder.
enum class RecordEvent : std::uint8_t
{
    kSyncReceived = 0,     ///< A Sync message was received and processed.
    kPdelayCompleted = 1,  ///< A full peer delay measurement cycle completed.
    kClockJump = 2,        ///< A forward or backward time jump was detected.
    kOffsetThreshold = 3,  ///< Clock offset exceeded offset_threshold_ns.
    kProbe = 4,  ///< Forwarded from ProbeManager::Trace(); status_flags column carries the ``ProbePoint`` value.
};

/// @brief A single record entry written to the CSV log file.
struct RecordEntry
{
    std::int64_t mono_ns{0};
    RecordEvent event{RecordEvent::kSyncReceived};
    std::int64_t offset_ns{0};
    std::int64_t pdelay_ns{0};
    std::uint16_t seq_id{0};
    std::uint8_t status_flags{0};
};

/// @brief Thread-safe CSV file recorder for gPTP events and diagnostics.
///
/// When enabled, appends one CSV row per event to the configured file path.
/// The file is opened in append mode; a header row is written only if the file
/// is newly created. CSV format:
/// @code
///   mono_ns,event,offset_ns,pdelay_ns,seq_id,status_flags
///   1234567890,0,1500,250000,42,3
/// @endcode
///
/// On write or flush failure, @c enabled_ is set to @c false atomically.
/// Failures are non-recoverable; subsequent @c Record() calls are no-ops.
/// The file is never re-opened after an error.
///
/// @see RecordEvent Enumeration of recordable event types.
/// @see RecordEntry Single CSV row data structure.
class Recorder final
{
  public:
    /// @brief Configuration parameters for the @c Recorder.
    struct Config
    {
        bool enabled = false;                                ///< Enable or disable recording.
        std::string file_path = "/var/log/gptp_record.csv";  ///< Output CSV file path.
        std::int64_t offset_threshold_ns =
            1'000'000LL;  ///< Reserved for ``kOffsetThreshold`` events (threshold above which offsets are logged); 1ms.
        std::uint32_t flush_interval = 8U;  ///< Number of rows between explicit ``file_.flush()`` calls.
    };

    explicit Recorder(Config cfg);
    ~Recorder() = default;

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    bool IsEnabled() const
    {
        return enabled_.load(std::memory_order_relaxed) && file_.is_open();
    }

    /// @brief Records an entry to the CSV file. Thread-safe.
    ///
    /// Serialises writes via @c mutex_. Appends one CSV line in the format
    /// @c mono_ns,event,offset_ns,pdelay_ns,seq_id,status_flags.
    /// Every @c Config::flush_interval rows, calls @c std::ofstream::flush().
    /// On write or flush failure sets @c enabled_ to @c false; later calls
    /// become no-ops.
    void Record(const RecordEntry& entry);

  private:
    Config cfg_;
    std::atomic<bool> enabled_{false};
    std::mutex mutex_;
    std::ofstream file_;
    std::uint32_t flush_counter_{0U};
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_SRC_GPTP_RECORD_RECORDER_H
