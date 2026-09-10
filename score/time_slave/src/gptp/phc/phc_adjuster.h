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
#ifndef SCORE_TIME_SLAVE_SRC_GPTP_PHC_PHC_ADJUSTER_H
#define SCORE_TIME_SLAVE_SRC_GPTP_PHC_PHC_ADJUSTER_H

#include <cstdint>
#include <string>

namespace score
{
namespace ts
{
namespace details
{

/// @brief Configuration for PHC hardware clock synchronisation.
///
/// The @c device field is platform-specific:
///   - Linux: path to the PHC character device, e.g. @c /dev/ptp0
///   - QNX: network interface name, e.g. @c emac0
struct PhcConfig
{
    bool enabled = false;                            ///< Enable or disable PHC adjustment. Default: @c false
    std::string device = "";                         ///< PHC device identifier. QNX: @c "emac0"; Linux: @c "/dev/ptp0"
    std::int64_t step_threshold_ns = 100'000'000LL;  ///< Offset above which a step correction is applied instead of
                                                     ///< frequency slew (ns). Default: 100\,000\,000 (100 ms)
};

/// @brief Adjusts the PTP Hardware Clock (PHC) on the NIC based on gPTP offset and rate.
///
/// When @c PhcConfig::enabled is @c true, applies step corrections for large
/// offsets and frequency slew for continuous tracking. When disabled, all
/// methods are no-ops.
///
/// Platform-specific implementations:
///   - @b Linux: @c clock_adjtime() (@c SYS_clock_adjtime syscall);
///     step via @c ADJ_SETOFFSET|ADJ_NANO; slew via @c ADJ_FREQUENCY (scaled-ppm).
///   - @b QNX: @c SIOCGDRVSPEC / @c SIOCSDRVSPEC on a UDP socket;
///     step via @c PTP_GET_TIME (0x102) + @c PTP_SET_TIME (0x103);
///     slew via @c EMAC_PTP_ADJ_FREQ_PPM (0x200) in ppm.
///
/// @b Fallback when PHC is unavailable:
/// On Linux the constructor calls @c open(device, O_RDWR); on failure
/// @c phc_fd_ stays at @c -1 and both @c AdjustOffset() and
/// @c AdjustFrequency() guard against @c phc_fd_ < 0 and return immediately.
/// The gPTP protocol pipeline (Sync/FollowUp reception, peer-delay, snapshot
/// publishing) is completely unaffected — only the NIC hardware clock itself
/// will drift relative to PTP time.
class PhcAdjuster final
{
  public:
    explicit PhcAdjuster(PhcConfig cfg);
    ~PhcAdjuster() noexcept;

    PhcAdjuster(const PhcAdjuster&) = delete;
    PhcAdjuster& operator=(const PhcAdjuster&) = delete;

    /// @brief Returns @c true if hardware clock adjustment is enabled.
    bool IsEnabled() const
    {
        return cfg_.enabled;
    }

    /// @brief Applies a time step or frequency slew based on offset magnitude.
    ///
    /// If |@p offset_ns| > @c step_threshold_ns a step correction is applied;
    /// otherwise the offset is ignored and frequency slew handles residual drift.
    /// No-op when PHC is disabled or @c phc_fd_ < 0.
    void AdjustOffset(std::int64_t offset_ns);

    /// @brief Adjusts the PHC frequency to track the master clock rate.
    ///
    /// @param rate_ratio  @c neighborRateRatio from @c SyncStateMachine (1.0 = no drift).
    /// No-op when PHC is disabled or @c phc_fd_ < 0.
    void AdjustFrequency(double rate_ratio);

  private:
    PhcConfig cfg_;
    int phc_fd_{-1};
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TIME_SLAVE_SRC_GPTP_PHC_PHC_ADJUSTER_H
