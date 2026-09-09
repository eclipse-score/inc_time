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

#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace score
{
namespace time
{
namespace detail
{

/// @brief Thread-safe holder for a single move-only callback that is invoked from a dedicated worker thread
///        whenever the observed value changes.
///
/// The slot remembers the @p Key of the last value delivered to the current callback, so
/// @c InvokeIfChanged() delivers only on change.  @c Set() forgets that key together with the old
/// callback: the first @c InvokeIfChanged() after any (re-)registration therefore always delivers.
///
/// Guarantees:
///  - @c Set() / @c Unset() may be called from any thread at any time.
///  - @c InvokeIfChanged() must be called from a single worker thread only.  The callback runs while
///    the slot's recursive mutex is held, so:
///     - @c Set() / @c Unset() from another thread block until the in-flight invocation has returned.
///       Once they return, the previously stored callback is neither running nor will it ever be
///       invoked again — the caller may safely destroy whatever the callback referenced.
///     - @c Set() / @c Unset() called re-entrantly from inside the callback take effect immediately;
///       the running invocation completes normally on a shared handle that outlives the slot contents.
///
/// @tparam Callback  A callable wrapper offering @c empty() and @c operator() (e.g. @c score::cpp::callback).
/// @tparam Key       Equality-comparable, copyable type identifying the value last delivered.
template <typename Callback, typename Key>
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
    /// Forgets the last delivered key, so the next @c InvokeIfChanged() delivers unconditionally.
    void Set(Callback&& callback) noexcept
    {
        const std::lock_guard<std::recursive_mutex> lock{mutex_};
        callback_ = callback.empty() ? nullptr : std::make_shared<Callback>(std::move(callback));
        last_key_.reset();
    }

    /// @brief Removes the stored callback.
    void Unset() noexcept
    {
        const std::lock_guard<std::recursive_mutex> lock{mutex_};
        callback_.reset();
        last_key_.reset();
    }

    /// @brief Returns @c true if a callback is currently installed.
    bool IsSet() const noexcept
    {
        const std::lock_guard<std::recursive_mutex> lock{mutex_};
        return callback_ != nullptr;
    }

    /// @brief Invokes the stored callback with @p argument unless @p key equals the key of the
    ///        previous delivery to the same callback.
    ///
    /// Must be called from the worker thread only.
    ///
    /// @return @c true if the callback was invoked, @c false if none is installed or @p key is unchanged.
    template <typename Argument>
    bool InvokeIfChanged(const Key& key, const Argument& argument) noexcept
    {
        const std::lock_guard<std::recursive_mutex> lock{mutex_};
        if ((callback_ == nullptr) || (last_key_.has_value() && (last_key_.value() == key)))
        {
            return false;
        }
        last_key_ = key;

        // Local copy keeps the callback alive should it Unset() or replace itself while running.
        const std::shared_ptr<Callback> callback = callback_;
        (*callback)(argument);
        return true;
    }

  private:
    mutable std::recursive_mutex mutex_;
    std::shared_ptr<Callback> callback_{};
    std::optional<Key> last_key_{};
};

}  // namespace detail
}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_VEHICLE_TIME_SRC_DETAILS_TD_IMPL_CALLBACK_SLOT_H
