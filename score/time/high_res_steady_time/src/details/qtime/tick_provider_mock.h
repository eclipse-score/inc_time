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
#ifndef SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_QTIME_TICK_PROVIDER_MOCK_H
#define SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_QTIME_TICK_PROVIDER_MOCK_H

#include "score/time/high_res_steady_time/src/details/qtime/tick_provider.h"

#include <gmock/gmock.h>
#include <cassert>
#include <memory>
#include <mutex>

namespace score
{
namespace time
{
namespace high_res_steady_time
{
namespace qtime
{

/// @brief Singleton GMock replacement for @c GetClockCyclesPerSec().
///
/// Usage:
/// @code
///   TickProviderMock::CreateMockInstance();
///   EXPECT_CALL(TickProviderMock::GetMockInstance(), GetClockCyclesPerSec()).WillOnce(Return(1'000'000'000U));
///   // ... run code under test ...
///   TickProviderMock::DestroyMockInstance();
/// @endcode
class TickProviderMock
{
  public:
    MOCK_METHOD(std::uint64_t, GetClockCyclesPerSec, (), ());

    static TickProviderMock& GetMockInstance() noexcept
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        assert(mock_instance_.operator bool());
        return *mock_instance_;
    }

    static void CreateMockInstance()
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        mock_instance_ = std::make_unique<TickProviderMock>();
    }

    static void DestroyMockInstance() noexcept
    {
        const std::lock_guard<std::mutex> guard{mutex_};
        testing::Mock::VerifyAndClearExpectations(mock_instance_.get());
        mock_instance_.reset();
    }

  private:
    static std::unique_ptr<TickProviderMock> mock_instance_;
    static std::mutex mutex_;
};

}  // namespace qtime
}  // namespace high_res_steady_time
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_HIGH_RES_STEADY_TIME_SRC_DETAILS_QTIME_TICK_PROVIDER_MOCK_H
