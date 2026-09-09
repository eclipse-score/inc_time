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
#ifndef EXAMPLES_TIME_VEHICLE_TIME_VEHICLE_TIME_HANDLER_H
#define EXAMPLES_TIME_VEHICLE_TIME_VEHICLE_TIME_HANDLER_H

#include "score/time/high_res_steady_time/src/high_res_steady_clock.h"
#include "score/time/vehicle_time/src/vehicle_clock.h"

#include <cstdint>

namespace examples
{
namespace time
{
namespace vehicle_time
{

/// @brief A snapshot of the combined time report produced by VehicleTimeHandler.
struct TimeReport
{
    /// @brief Current vehicle (PTP-synchronized) time, nanoseconds since VehicleTime epoch.
    std::int64_t vehicle_time_ns{0};

    /// @brief Current local monotonic time from HighResSteadyClock, nanoseconds since process boot.
    ///        Provided as a local-time reference alongside the network-synchronized vehicle time.
    std::int64_t high_res_steady_time_ns{0};

    /// @brief True if the vehicle time data is currently reliable (synchronized + no faults active).
    bool is_reliable{false};

    /// @brief True if the vehicle time status flags are internally consistent (no contradictory combination).
    bool is_consistent{false};

    /// @brief Fractional rate deviation of the local clock relative to the PTP Grand Master.
    ///        Unit: dimensionless (e.g. 1.0e-9 == 1 ppb).
    double rate_deviation{0.0};
};

/// @brief Convenience wrapper that reads VehicleClock and HighResSteadyClock in one call.
///
/// This class demonstrates how to write a component that depends on two different
/// time bases.  In unit tests, both clocks can be replaced independently via
/// ScopedClockOverride, without any special constructor injection.
///
/// @par Testing pattern
/// @code
///   auto vehicle_mock = std::make_shared<score::time::VehicleClockBackendMock>();
///   auto hirs_mock    = std::make_shared<score::time::HighResSteadyClockBackendMock>();
///   score::time::test_utils::ScopedClockOverride<score::time::VehicleTime> vg{vehicle_mock};
///   score::time::test_utils::ScopedClockOverride<score::time::HighResSteadyTime>    hg{hirs_mock};
///   EXPECT_CALL(*vehicle_mock, Init()).WillOnce(Return(true));
///   EXPECT_CALL(*vehicle_mock, Now()).WillOnce(Return(...));
///   EXPECT_CALL(*hirs_mock,    Now()).WillOnce(Return(...));
///   VehicleTimeHandler handler;
///   handler.Init();
///   const auto report = handler.GetCurrentTime();
/// @endcode
class VehicleTimeHandler
{
  public:
    VehicleTimeHandler()
        : clock_{score::time::VehicleClock::GetInstance()},
          high_res_steady_clock_{score::time::HighResSteadyClock::GetInstance()}
    {
    }
    ~VehicleTimeHandler() = default;

    VehicleTimeHandler(const VehicleTimeHandler&) = delete;
    VehicleTimeHandler& operator=(const VehicleTimeHandler&) = delete;
    VehicleTimeHandler(VehicleTimeHandler&&) = delete;
    VehicleTimeHandler& operator=(VehicleTimeHandler&&) = delete;

    /// @brief Initialises the VehicleTime backend.
    ///
    /// Must be called once before @c GetCurrentTime(). Returns @c false on failure;
    /// the call may be retried. @c GetCurrentTime() returns @c kUnknown snapshots
    /// until this succeeds.
    ///
    /// @return @c true if the backend is ready.
    [[nodiscard]] bool Init() noexcept
    {
        return clock_.Init();
    }

    /// @brief Reads the current vehicle time and local HIRS time and returns a combined report.
    [[nodiscard]] TimeReport GetCurrentTime() const noexcept
    {
        const auto vehicle_snapshot = clock_.Now();
        const auto hirs_snapshot = high_res_steady_clock_.Now();

        return TimeReport{
            vehicle_snapshot.TimePointNs().count(),
            hirs_snapshot.TimePointNs().count(),
            vehicle_snapshot.Status().IsReliable(),
            vehicle_snapshot.Status().IsConsistent(),
            vehicle_snapshot.Status().RateDeviation(),
        };
    }

    /// @brief Registers a callback that is invoked when VehicleTimeStatus flags change.
    ///
    /// The callback fires once with the current status right after registration and
    /// afterwards whenever the status flags change.  It is invoked on the backend's
    /// worker thread, so the callback implementation must be thread-safe.
    void RegisterStatusCallback(score::time::VehicleTime::StatusChangedCallback callback) noexcept
    {
        clock_.Subscribe<score::time::VehicleTimeStatus>(std::move(callback));
    }

    /// @brief Removes the status-change callback.
    void UnregisterStatusCallback() noexcept
    {
        clock_.Unsubscribe<score::time::VehicleTimeStatus>();
    }

  private:
    score::time::VehicleClock clock_;
    score::time::HighResSteadyClock high_res_steady_clock_;
};

}  // namespace vehicle_time
}  // namespace time
}  // namespace examples

#endif  // EXAMPLES_TIME_VEHICLE_TIME_VEHICLE_TIME_HANDLER_H
