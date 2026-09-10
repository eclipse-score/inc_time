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
#include "score/time/vehicle_time/src/details/td_impl/svt_callback_wrapper.h"

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
using Slot = SvtCallbackWrapper<TestCallback, int>;

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

TEST(SvtCallbackWrapperTest, InvokeIfChangedReturnsFalseWhenNoCallbackIsSet)
{
    Slot slot;
    EXPECT_FALSE(slot.IsSet());
    EXPECT_FALSE(slot.InvokeIfChanged(1, 1));
}

TEST(SvtCallbackWrapperTest, InvokeIfChangedCallsStoredCallbackWithArgumentOnEveryNewKey)
{
    Slot slot;
    std::vector<int> received;
    slot.Set([&received](const int& value) {
        received.push_back(value);
    });

    EXPECT_TRUE(slot.IsSet());
    EXPECT_TRUE(slot.InvokeIfChanged(7, 70));
    EXPECT_TRUE(slot.InvokeIfChanged(8, 80));
    EXPECT_EQ(received, (std::vector<int>{70, 80}));
}

TEST(SvtCallbackWrapperTest, InvokeIfChangedSkipsRepeatedKey)
{
    Slot slot;
    int invocations{0};
    slot.Set([&invocations](const int&) {
        ++invocations;
    });

    EXPECT_TRUE(slot.InvokeIfChanged(7, 7));
    EXPECT_FALSE(slot.InvokeIfChanged(7, 7));
    EXPECT_FALSE(slot.InvokeIfChanged(7, 8));  // argument differs but key does not
    EXPECT_TRUE(slot.InvokeIfChanged(9, 9));
    EXPECT_EQ(invocations, 2);
}

TEST(SvtCallbackWrapperTest, SetForgetsLastKeySoNewCallbackIsInvokedWithUnchangedKey)
{
    Slot slot;
    slot.Set([](const int&) {});
    EXPECT_TRUE(slot.InvokeIfChanged(7, 7));
    EXPECT_FALSE(slot.InvokeIfChanged(7, 7));

    int replacement_invocations{0};
    slot.Set([&replacement_invocations](const int&) {
        ++replacement_invocations;
    });
    EXPECT_TRUE(slot.InvokeIfChanged(7, 7));
    EXPECT_EQ(replacement_invocations, 1);
}

TEST(SvtCallbackWrapperTest, UnsetRemovesCallbackAndForgetsLastKey)
{
    Slot slot;
    int invocations{0};
    slot.Set([&invocations](const int&) {
        ++invocations;
    });
    EXPECT_TRUE(slot.InvokeIfChanged(7, 7));
    slot.Unset();

    EXPECT_FALSE(slot.IsSet());
    EXPECT_FALSE(slot.InvokeIfChanged(7, 7));

    slot.Set([&invocations](const int&) {
        ++invocations;
    });
    EXPECT_TRUE(slot.InvokeIfChanged(7, 7));
    EXPECT_EQ(invocations, 2);
}

TEST(SvtCallbackWrapperTest, SettingEmptyCallbackBehavesLikeUnset)
{
    Slot slot;
    slot.Set([](const int&) {});
    slot.Set(TestCallback{});

    EXPECT_FALSE(slot.IsSet());
    EXPECT_FALSE(slot.InvokeIfChanged(0, 0));
}

TEST(SvtCallbackWrapperTest, UnsetFromWithinCallbackDoesNotDeadlockAndTakesEffectAfterwards)
{
    Slot slot;
    int invocations{0};
    slot.Set([&slot, &invocations](const int&) {
        ++invocations;
        slot.Unset();
    });

    EXPECT_TRUE(slot.InvokeIfChanged(0, 0));
    EXPECT_FALSE(slot.IsSet());
    EXPECT_FALSE(slot.InvokeIfChanged(0, 0));
    EXPECT_EQ(invocations, 1);
}

TEST(SvtCallbackWrapperTest, SetFromWithinCallbackReplacesCallbackForNextInvocation)
{
    Slot slot;
    std::vector<int> trace;
    slot.Set([&slot, &trace](const int&) {
        trace.push_back(1);
        slot.Set([&trace](const int&) {
            trace.push_back(2);
        });
    });

    EXPECT_TRUE(slot.InvokeIfChanged(0, 0));
    EXPECT_TRUE(slot.InvokeIfChanged(0, 0));
    EXPECT_EQ(trace, (std::vector<int>{1, 2}));
}

TEST(SvtCallbackWrapperTest, UnsetFromAnotherThreadBlocksUntilInFlightInvocationReturns)
{
    Slot slot;
    Gate callback_entered;
    Gate release_callback;
    slot.Set([&callback_entered, &release_callback](const int&) {
        callback_entered.Open();
        release_callback.Wait();
    });

    std::thread invoker{[&slot]() {
        std::ignore = slot.InvokeIfChanged(0, 0);
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

TEST(SvtCallbackWrapperTest, SetFromAnotherThreadBlocksUntilInFlightInvocationReturns)
{
    Slot slot;
    Gate callback_entered;
    Gate release_callback;
    slot.Set([&callback_entered, &release_callback](const int&) {
        callback_entered.Open();
        release_callback.Wait();
    });

    std::thread invoker{[&slot]() {
        std::ignore = slot.InvokeIfChanged(0, 0);
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

    EXPECT_TRUE(slot.InvokeIfChanged(0, 0));
    EXPECT_EQ(replacement_invocations, 1);
}

}  // namespace
}  // namespace detail
}  // namespace time
}  // namespace score
