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
#ifndef SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_RAW_SOCKET_H
#define SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_RAW_SOCKET_H

#include <time.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace score
{
namespace ts
{
namespace details
{

/// @brief Platform-agnostic raw socket interface used by @c GptpEngine and
/// @c PeerDelayMeasurer for gPTP frame transmission and reception.
///
/// Provides hardware timestamping support with automatic fallback to software
/// timestamps when the NIC does not support hardware timestamping.
class RawSocket
{
  public:
    virtual ~RawSocket() noexcept = default;

    /// @brief Open the socket bound to @p iface.
    ///
    /// @return false on failure.
    virtual bool Open(const std::string& iface) = 0;

    /// @brief Configures hardware TX/RX timestamping on the NIC if supported.
    ///
    /// @return false on failure or if hardware timestamping is unsupported.
    virtual bool EnableHwTimestamping() = 0;

    /// @brief Close the socket and release the file descriptor.
    virtual void Close() = 0;

    /// @brief Receive one Ethernet frame with its hardware (or software fallback) timestamp.
    ///
    /// @param buf        Buffer for the received frame.
    /// @param buf_len    Buffer length in bytes.
    /// @param hwts       Output: hardware timestamp if available, otherwise software timestamp.
    /// @param timeout_ms Receive timeout in milliseconds (0 = non-blocking).
    /// @return Number of bytes received, 0 on timeout, -1 on error.
    virtual int Recv(std::uint8_t* buf, std::size_t buf_len, ::timespec& hwts, int timeout_ms) = 0;

    /// @brief Send one Ethernet frame and capture its hardware transmit timestamp.
    ///
    /// @param buf  Frame buffer.
    /// @param len  Frame length in bytes.
    /// @param hwts Output: hardware transmit timestamp if available, otherwise software timestamp.
    /// @return Number of bytes sent, or -1 on error.
    virtual int Send(const void* buf, int len, ::timespec& hwts) = 0;

    /// @brief Return the underlying file descriptor (for use with @c select / @c poll).
    virtual int GetFd() const = 0;
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_RAW_SOCKET_H
