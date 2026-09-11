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
#ifndef SCORE_TIME_PTP_SRC_TIME_SLAVE_SYNC_DATA_H
#define SCORE_TIME_PTP_SRC_TIME_SLAVE_SYNC_DATA_H

#include "score/time/ptp/src/local_ptp_device_timer.h"
#include "score/time/ptp/src/port_identity.h"

#include <chrono>
#include <cstdint>
#include <ostream>

namespace score
{
namespace time
{

/// @brief Data delivered when new time-synchronization information is received by a Time Slave.
///
/// @tparam Timebase  The clock domain tag (e.g. @c VehicleTime).
template <typename Timebase>
struct TimeSlaveSyncData
{
    /// @brief Time Master's timestamp of global time at which a Sync Frame has actually been transmitted
    ///        at its Ethernet port and which gets put into the Sync Follow-Up Frame sent afterwards.
    typename Timebase::Timepoint precise_origin_timestamp{};

    /// @brief Time Slave's timestamp of global time after the sync process took place.
    typename Timebase::Timepoint reference_global_timestamp{};

    /// @brief Time Slave's local time (e.g. Ethernet device's HW timer) after the sync process took place.
    LocalPTPDeviceTimerValue reference_local_timestamp{};

    /// @brief Time Slave's local time at the point where it received the Sync Frame at its Ethernet port.
    LocalPTPDeviceTimerValue sync_ingress_timestamp{};

    /// @brief Correction value taken from the Sync Follow-Up Frame.
    ///
    /// Unit: 1 / 0x10000 nanoseconds (i.e. a value of 0x10000 == 1 nanosecond).
    std::int64_t correction_field{};

    /// @brief Sequence number of the received Sync Frame.
    std::uint16_t sequence_id{};

    /// @brief Currently valid pDelay value.
    std::chrono::nanoseconds pdelay{};

    /// @brief Identity of the port the Sync & Follow-Up Frames originate from.
    PortIdentity source_port_identity{};

    /// @brief Prints the data to any output stream.
    ///
    /// @tparam OutputStream  Type of the output stream (e.g. std::ostream).
    /// @param output_stream  The output stream to print to.
    /// @return Reference to the output stream for chaining.
    template <typename OutputStream>
    auto& PrintTo(OutputStream& output_stream) const
    {
        /* False positives: << incorrectly identified as bitwise operators, no address of a local variable is
         * returned. */
        return output_stream << "[" << precise_origin_timestamp.time_since_epoch().count() << ", "
                             << reference_global_timestamp.time_since_epoch().count() << ", "
                             << reference_local_timestamp.time_since_epoch().count() << ", "
                             << sync_ingress_timestamp.time_since_epoch().count() << ", " << correction_field
                             << " / 0x10000, " << sequence_id << ", " << pdelay.count() << ", " << source_port_identity
                             << "]";
    }
};

/// @brief Stream output operator for @c TimeSlaveSyncData.
template <typename OutputStream, typename Timebase>
auto& operator<<(OutputStream& output_stream, const TimeSlaveSyncData<Timebase>& sync_data)
{
    return sync_data.PrintTo(output_stream);
}

/// @brief GTest PrintTo overload for @c TimeSlaveSyncData.
template <typename Timebase>
/* The dereference output_stream is passed as a non-const reference. Therefore, output_stream should be a pointer to a
 * non-const. */
void PrintTo(const TimeSlaveSyncData<Timebase>& sync_data, std::ostream* const output_stream)
{
    sync_data.PrintTo(*output_stream);
}

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_PTP_SRC_TIME_SLAVE_SYNC_DATA_H
