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
#include "score/time/vehicle_time/src/vehicle_clock.h"
#include "score/time/vehicle_time/src/vehicle_clock_backend.h"

#include <map>
#include <string>
#include <utility>

namespace score
{
namespace time
{

template <>
std::ostringstream ClockStatus<VehicleTime::StatusFlag>::PrintTo() const
{
    static const std::map<VehicleTime::StatusFlag, std::string> kFlagNames = {
        {VehicleTime::StatusFlag::kTimeOut, "kTimeOut"},
        {VehicleTime::StatusFlag::kSynchronized, "kSynchronized"},
        {VehicleTime::StatusFlag::kTimeLeapFuture, "kTimeLeapFuture"},
        {VehicleTime::StatusFlag::kTimeLeapPast, "kTimeLeapPast"},
    };
    std::ostringstream oss;
    oss << "[";
    for (const auto& entry : kFlagNames)
    {
        oss << entry.second << ": " << (IsFlagActive(entry.first) ? "true" : "false") << ", ";
    }
    oss << "]";
    return oss;
}

// # req-Id: comp_req__vehicle_time__snapshot
ClockTraits<VehicleTime>::Snapshot ClockTraits<VehicleTime>::CallNow(const Backend& impl) noexcept
{
    return impl.Now();
}

// # req-Id: comp_req__vehicle_time__lifecycle
bool InitializationHook<VehicleTime>::CallInit(Backend& impl) noexcept
{
    return impl.Init();
}

// # req-Id: comp_req__vehicle_time__lifecycle
bool AvailabilityHook<VehicleTime>::CallIsAvailable(const Backend& impl) noexcept
{
    return impl.IsAvailable();
}

// # req-Id: comp_req__vehicle_time__lifecycle
bool AvailabilityHook<VehicleTime>::CallWaitUntilAvailable(const Backend& impl,
                                                           const score::cpp::stop_token& token,
                                                           std::chrono::steady_clock::time_point until) noexcept
{
    return impl.WaitUntilAvailable(token, until);
}

void SubscriptionHook<VehicleTime, TimeSlaveSyncData<VehicleTime>>::Subscribe(
    Backend& impl,
    VehicleTime::TimeSlaveSyncDataReceivedCallback cb) noexcept
{
    impl.SetTimeSlaveSyncDataReceivedCallback(std::move(cb));
}

void SubscriptionHook<VehicleTime, TimeSlaveSyncData<VehicleTime>>::Unsubscribe(Backend& impl) noexcept
{
    impl.UnsetTimeSlaveSyncDataReceivedCallback();
}

void SubscriptionHook<VehicleTime, PDelayMeasurementData<VehicleTime>>::Subscribe(
    Backend& impl,
    VehicleTime::PDelayMeasurementFinishedCallback cb) noexcept
{
    impl.SetPDelayMeasurementFinishedCallback(std::move(cb));
}

void SubscriptionHook<VehicleTime, PDelayMeasurementData<VehicleTime>>::Unsubscribe(Backend& impl) noexcept
{
    impl.UnsetPDelayMeasurementFinishedCallback();
}

void SubscriptionHook<VehicleTime, VehicleTimeStatus>::Subscribe(Backend& impl,
                                                                 VehicleTime::StatusChangedCallback cb) noexcept
{
    impl.SetStatusChangedCallback(std::move(cb));
}

void SubscriptionHook<VehicleTime, VehicleTimeStatus>::Unsubscribe(Backend& impl) noexcept
{
    impl.UnsetStatusChangedCallback();
}

}  // namespace time
}  // namespace score
