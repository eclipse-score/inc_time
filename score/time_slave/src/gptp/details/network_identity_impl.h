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
#ifndef SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_NETWORK_IDENTITY_IMPL_H
#define SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_NETWORK_IDENTITY_IMPL_H

#include "score/time_slave/src/gptp/details/network_identity.h"
#include "score/time_slave/src/gptp/details/ptp_types.h"

#include <string>

namespace score
{
namespace ts
{
namespace details
{

/**
 * @brief Derive the IEEE 1588 ClockIdentity from a network interface.
 *
 * The identity is built from the interface's EUI-48 MAC address by inserting
 * 0xFF 0xFE at positions 3–4 to form an EUI-64 (per IEEE 1588-2019 §7.5.2.2).
 * Platform implementation: Linux + QNX via #ifdef.
 */
class NetworkIdentityImpl : public NetworkIdentity
{
  public:
    /// @brief Resolve the ClockIdentity for @p iface_name.
    /// @return true on success.
    bool Resolve(const std::string& iface_name) override;

    /// @brief Return the resolved identity.  Valid only after a successful Resolve().
    ClockIdentity GetClockIdentity() const override
    {
        return identity_;
    }

  private:
    ClockIdentity identity_{};
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_SRC_GPTP_DETAILS_NETWORK_IDENTITY_IMPL_H
