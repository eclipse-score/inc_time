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
#ifndef SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_CALLBACK_SLOT_H
#define SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_CALLBACK_SLOT_H

// Internal header — include ONLY from translation units under vehicle_time/src/details/td_impl/.
// NOT part of the public API of td_impl.

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace score
{
namespace time
{
namespace detail
{

/// @brief Thread-safe holder for a single move-only callback that is invoked from a dedicated worker thread.
///
/// Guarantees:
///  - @c Set() / @c Unset() may be called from any thread at any time.
///  - @c Invoke() must be called from a single worker thread only. The stored callback is invoked
///    @b without holding the slot mutex, so a callback may itself call @c Set() / @c Unset() on this
///    or on any other slot.
///  - @c Set() / @c Unset() called from a thread other than the one currently running the callback
///    block until the in-flight invocation has returned. Once they return, the previously stored
///    callback is neither running nor will it ever be invoked again — the caller may safely destroy
///    whatever the callback referenced.
///  - @c Set() / @c Unset() called re-entrantly from inside the callback return immediately; the
///    running invocation completes normally (it operates on a shared handle that outlives the slot
///    contents).
///  - Every successful @c Set() increments @c Generation(), which lets the worker detect a
///    (re-)registration and apply "first event after registration" semantics.
///
/// @tparam Callback  A callable wrapper offering @c empty() and @c operator() (e.g. @c score::cpp::callback).
template <typename Callback>
class CallbackSlot final
{
  public:
    CallbackSlot() noexcept = default;
    ~CallbackSlot() noexcept = default;
    CallbackSlot(const CallbackSlot&) = delete;
    CallbackSlot& operator=(const CallbackSlot&) = delete;
    CallbackSlot(CallbackSlot&&) = delete;
    CallbackSlot& operator=(CallbackSlot&&) = delete;

    /// @brief Installs @p callback, replacing any previous one. An empty callback behaves like @c Unset().
    ///
    /// Blocks until an in-flight invocation of the previous callback has returned, unless called from
    /// within that invocation.
    void Set(Callback&& callback) noexcept
    {
        std::shared_ptr<Callback> fresh{};
        if (!callback.empty())
        {
            fresh = std::make_shared<Callback>(std::move(callback));
        }

        std::unique_lock<std::mutex> lock{mutex_};
        callback_ = std::move(fresh);
        if (callback_ != nullptr)
        {
            ++generation_;
        }
        WaitForInFlightInvocation(lock);
    }

    /// @brief Removes the stored callback.
    ///
    /// Blocks until an in-flight invocation has returned, unless called from within that invocation.
    void Unset() noexcept
    {
        std::unique_lock<std::mutex> lock{mutex_};
        callback_.reset();
        WaitForInFlightInvocation(lock);
    }

    /// @brief Returns @c true if a callback is currently installed.
    bool IsSet() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        return callback_ != nullptr;
    }

    /// @brief Returns the registration counter, incremented on every successful @c Set().
    std::uint64_t Generation() const noexcept
    {
        const std::lock_guard<std::mutex> lock{mutex_};
        return generation_;
    }

    /// @brief Invokes the stored callback with @p argument, if one is installed.
    ///
    /// Must be called from the worker thread only. The callback runs without the slot mutex held.
    ///
    /// @return The generation of the callback that was invoked, or @c std::nullopt if none was installed.
    template <typename Argument>
    std::optional<std::uint64_t> Invoke(const Argument& argument) noexcept
    {
        std::shared_ptr<Callback> callback{};
        std::uint64_t invoked_generation{0U};
        {
            const std::lock_guard<std::mutex> lock{mutex_};
            if (callback_ == nullptr)
            {
                return std::nullopt;
            }
            callback = callback_;
            invoked_generation = generation_;
            invoking_thread_ = std::this_thread::get_id();
        }

        (*callback)(argument);

        {
            // Notify while still holding the lock: a waiter in Set()/Unset() can only resume once we
            // released it, so the slot may be destroyed right after Set()/Unset() return.
            const std::lock_guard<std::mutex> lock{mutex_};
            invoking_thread_ = std::thread::id{};
            invocation_finished_.notify_all();
        }
        return invoked_generation;
    }

  private:
    /// @brief Waits until no invocation is in flight; returns immediately when called from the invoking thread.
    void WaitForInFlightInvocation(std::unique_lock<std::mutex>& lock) noexcept
    {
        if (invoking_thread_ == std::this_thread::get_id())
        {
            return;
        }
        invocation_finished_.wait(lock, [this]() noexcept {
            return invoking_thread_ == std::thread::id{};
        });
    }

    mutable std::mutex mutex_;
    std::condition_variable invocation_finished_;
    std::shared_ptr<Callback> callback_{};
    std::uint64_t generation_{0U};
    std::thread::id invoking_thread_{};
};

}  // namespace detail
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_CALLBACK_SLOT_H
