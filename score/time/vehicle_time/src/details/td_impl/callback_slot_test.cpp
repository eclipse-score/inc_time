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
#include "score/time/vehicle_time/src/details/td_impl/callback_slot.h"

#include <score/callback.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace score
{
namespace time
{
namespace detail
{
namespace
{

using TestCallback = score::cpp::callback<void(const int&), 64U>;
using Slot = CallbackSlot<TestCallback>;

/// @brief One-shot gate that lets one thread block until another thread opens it.
class Gate
{
  public:
    void Open() noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        open_ = true;
        condition_.notify_all();
    }

    void Wait() noexcept
    {
        std::unique_lock<std::mutex> lock{mutex_};
        condition_.wait(lock, [this]() noexcept {
            return open_;
        });
    }

    bool WaitFor(const std::chrono::milliseconds timeout) noexcept
    {
        std::unique_lock<std::mutex> lock{mutex_};
        return condition_.wait_for(lock, timeout, [this]() noexcept {
            return open_;
        });
    }

  private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool open_{false};
};

TEST(CallbackSlotTest, InvokeReturnsNulloptWhenNoCallbackIsSet)
{
    Slot slot;
    EXPECT_FALSE(slot.IsSet());
    EXPECT_FALSE(slot.Invoke(1).has_value());
}

TEST(CallbackSlotTest, InvokeCallsStoredCallbackWithArgument)
{
    Slot slot;
    std::vector<int> received;
    slot.Set([&received](const int& value) {
        received.push_back(value);
    });

    EXPECT_TRUE(slot.IsSet());
    EXPECT_TRUE(slot.Invoke(7).has_value());
    EXPECT_TRUE(slot.Invoke(8).has_value());
    EXPECT_EQ(received, (std::vector<int>{7, 8}));
}

TEST(CallbackSlotTest, GenerationIncrementsOnEverySetAndIsReportedByInvoke)
{
    Slot slot;
    EXPECT_EQ(slot.Generation(), 0U);

    slot.Set([](const int&) {});
    EXPECT_EQ(slot.Generation(), 1U);
    EXPECT_EQ(slot.Invoke(0).value(), 1U);

    slot.Set([](const int&) {});
    EXPECT_EQ(slot.Generation(), 2U);
    EXPECT_EQ(slot.Invoke(0).value(), 2U);
}

TEST(CallbackSlotTest, UnsetRemovesCallbackWithoutChangingGeneration)
{
    Slot slot;
    slot.Set([](const int&) {});
    slot.Unset();

    EXPECT_FALSE(slot.IsSet());
    EXPECT_EQ(slot.Generation(), 1U);
    EXPECT_FALSE(slot.Invoke(0).has_value());
}

TEST(CallbackSlotTest, SettingEmptyCallbackBehavesLikeUnset)
{
    Slot slot;
    slot.Set([](const int&) {});
    slot.Set(TestCallback{});

    EXPECT_FALSE(slot.IsSet());
    EXPECT_EQ(slot.Generation(), 1U);
    EXPECT_FALSE(slot.Invoke(0).has_value());
}

TEST(CallbackSlotTest, UnsetFromWithinCallbackDoesNotDeadlockAndTakesEffectAfterwards)
{
    Slot slot;
    int invocations{0};
    slot.Set([&slot, &invocations](const int&) {
        ++invocations;
        slot.Unset();
    });

    EXPECT_TRUE(slot.Invoke(0).has_value());
    EXPECT_FALSE(slot.IsSet());
    EXPECT_FALSE(slot.Invoke(0).has_value());
    EXPECT_EQ(invocations, 1);
}

TEST(CallbackSlotTest, SetFromWithinCallbackReplacesCallbackForNextInvocation)
{
    Slot slot;
    std::vector<int> trace;
    slot.Set([&slot, &trace](const int&) {
        trace.push_back(1);
        slot.Set([&trace](const int&) {
            trace.push_back(2);
        });
    });

    EXPECT_EQ(slot.Invoke(0).value(), 1U);
    EXPECT_EQ(slot.Invoke(0).value(), 2U);
    EXPECT_EQ(trace, (std::vector<int>{1, 2}));
}

TEST(CallbackSlotTest, UnsetFromAnotherThreadBlocksUntilInFlightInvocationReturns)
{
    Slot slot;
    Gate callback_entered;
    Gate release_callback;
    slot.Set([&callback_entered, &release_callback](const int&) {
        callback_entered.Open();
        release_callback.Wait();
    });

    std::thread invoker{[&slot]() {
        std::ignore = slot.Invoke(0);
    }};
    callback_entered.Wait();

    auto unset_done = std::async(std::launch::async, [&slot]() {
        slot.Unset();
    });
    EXPECT_EQ(unset_done.wait_for(std::chrono::milliseconds{50}), std::future_status::timeout);

    release_callback.Open();
    EXPECT_EQ(unset_done.wait_for(std::chrono::seconds{5}), std::future_status::ready);
    invoker.join();
    EXPECT_FALSE(slot.IsSet());
}

TEST(CallbackSlotTest, SetFromAnotherThreadBlocksUntilInFlightInvocationReturns)
{
    Slot slot;
    Gate callback_entered;
    Gate release_callback;
    slot.Set([&callback_entered, &release_callback](const int&) {
        callback_entered.Open();
        release_callback.Wait();
    });

    std::thread invoker{[&slot]() {
        std::ignore = slot.Invoke(0);
    }};
    callback_entered.Wait();

    int replacement_invocations{0};
    auto set_done = std::async(std::launch::async, [&slot, &replacement_invocations]() {
        slot.Set([&replacement_invocations](const int&) {
            ++replacement_invocations;
        });
    });
    EXPECT_EQ(set_done.wait_for(std::chrono::milliseconds{50}), std::future_status::timeout);

    release_callback.Open();
    EXPECT_EQ(set_done.wait_for(std::chrono::seconds{5}), std::future_status::ready);
    invoker.join();

    EXPECT_EQ(slot.Invoke(0).value(), 2U);
    EXPECT_EQ(replacement_invocations, 1);
}

}  // namespace
}  // namespace detail
}  // namespace time
}  // namespace score
