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
#ifndef SCORE_TIME_PTP_SRC_PORT_IDENTITY_H
#define SCORE_TIME_PTP_SRC_PORT_IDENTITY_H

#include <cstdint>
#include <ostream>

namespace score
{
namespace time
{

/// @brief Identifies a participant (port) involved in PTP communication.
struct PortIdentity
{
    /// @brief Clock identity of the PTP port.
    std::uint64_t clock_identity{};

    /// @brief Port number of the PTP port.
    std::uint16_t port_number{};

    /// @brief Prints the identity to any output stream.
    ///
    /// @tparam OutputStream  Type of the output stream (e.g. std::ostream).
    /// @param output_stream  The output stream to print to.
    /// @return Reference to the output stream for chaining.
    template <typename OutputStream>
    auto& PrintTo(OutputStream& output_stream) const
    {
        /* False positives: << incorrectly identified as bitwise operators, no address of a local variable is
         * returned.*/
        return output_stream << "(" << clock_identity << ", " << port_number << ")";
    }
};

/// @brief Stream output operator for @c PortIdentity.
///
/// @tparam OutputStream  Type of the output stream (e.g. std::ostream).
/// @param output_stream  The output stream to print to.
/// @param port_identity  The @c PortIdentity to print.
/// @return Reference to the output stream for chaining.
template <typename OutputStream>
auto& operator<<(OutputStream& output_stream, const PortIdentity& port_identity)
{
    return port_identity.PrintTo(output_stream);
}

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_PTP_SRC_PORT_IDENTITY_H
