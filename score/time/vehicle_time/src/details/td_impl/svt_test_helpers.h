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
#ifndef SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_SVT_TEST_HELPERS_H
#define SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_SVT_TEST_HELPERS_H

// Test-only helpers shared between the SvtCallbackDispatcher and VehicleClockBackendImpl unit tests.

#include "score/time_daemon/src/ipc/receiver_mock.h"
#include "score/time_daemon/src/ipc/svt/svt_time_info.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>

namespace score
{
namespace time
{
namespace test_helpers
{

using SvtMock = score::td::ReceiverMock<score::td::svt::TimeBaseSnapshot>;
using SvtSnapshot = score::td::svt::TimeBaseSnapshot;
using SvtStatus = score::td::svt::TimeBaseStatus;
using SvtSyncData = score::td::svt::SyncFupSnapshot;
using SvtPDelayData = score::td::svt::PDelayDataSnapshot;

constexpr SvtStatus kSynchronizedStatus{true, false, false, false, true};
constexpr SvtStatus kTimeoutStatus{true, true, false, false, true};
constexpr std::chrono::milliseconds kPollInterval{1};
constexpr std::chrono::seconds kWaitTimeout{5};

inline SvtSnapshot MakeFrame(const SvtStatus status, const double rate_deviation = 0.0) noexcept
{
    return SvtSnapshot{1000ULL, 0ULL, rate_deviation, status, {}, {}};
}

/// @brief Thread-safe frame supplier for the mocked receiver; counts how often the worker polled.
class FrameSource
{
  public:
    void Set(const std::optional<SvtSnapshot>& frame) noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        frame_ = frame;
    }

    std::optional<SvtSnapshot> Get() noexcept
    {
        // Notify under the lock so the object can be destroyed as soon as a waiter resumes.
        const std::lock_guard<std::mutex> lock{mutex_};
        ++polls_;
        polled_.notify_all();
        return frame_;
    }

    std::size_t Polls() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        return polls_;
    }

    /// @brief Blocks until the worker polled at least @p additional more times than right now.
    bool WaitForAdditionalPolls(const std::size_t additional) noexcept
    {
        std::unique_lock<std::mutex> lock{mutex_};
        const std::size_t target = polls_ + additional;
        return polled_.wait_for(lock, kWaitTimeout, [this, target]() noexcept {
            return polls_ >= target;
        });
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable polled_;
    std::optional<SvtSnapshot> frame_{};
    std::size_t polls_{0U};
};

/// @brief Records callback invocations so the test thread can wait for them.
template <typename Event>
class Recorder
{
  public:
    void Record(const Event& event) noexcept
    {
        // Notify under the lock so the recorder can be destroyed as soon as a waiter resumes.
        const std::lock_guard<std::mutex> lock{mutex_};
        last_ = event;
        ++count_;
        recorded_.notify_all();
    }

    bool WaitForCount(const std::size_t count) noexcept
    {
        std::unique_lock<std::mutex> lock{mutex_};
        return recorded_.wait_for(lock, kWaitTimeout, [this, count]() noexcept {
            return count_ >= count;
        });
    }

    std::size_t Count() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        return count_;
    }

    Event Last() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        return last_;
    }

  private:
    mutable std::mutex mutex_;
    std::condition_variable recorded_;
    Event last_{};
    std::size_t count_{0U};
};

}  // namespace test_helpers
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_SVT_TEST_HELPERS_H
