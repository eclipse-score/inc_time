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
#ifndef SCORE_TIME_CLOCK_SRC_CLOCK_H
#define SCORE_TIME_CLOCK_SRC_CLOCK_H

#include "score/time/clock/src/availability_hook.h"
#include "score/time/clock/src/clock_traits.h"
#include "score/time/clock/src/initialization_hook.h"
#include "score/time/clock/src/subscription_hook.h"

#include <score/assert.hpp>
#include <score/stop_token.hpp>

#include <chrono>
#include <memory>
#include <mutex>

namespace score
{
namespace time
{

// Forward declarations for test-only helpers. Placed in their own sub-namespace
// so Clock<Tag> can friend them without polluting score::time.
namespace test_utils
{
template <typename Tag>
class ScopedClockOverride;

template <typename Tag>
class ClockTestFactory;
}  // namespace test_utils

namespace detail
{

/// @brief Link-selected backend factory.
///
/// Declared here; defined only in the link-selected backend implementation .cpp
/// Clients never call this directly —
/// only @c Clock<Tag>::GetInstance() does.
template <typename Tag>
std::shared_ptr<typename ClockTraits<Tag>::Backend> CreateBackend();

}  // namespace detail

/// @brief Unified clock-domain-agnostic API for reading time snapshots.
///
/// Separates what kind of time (tag struct: @c VehicleTime, @c HighResSteadyTime,
/// @c std::chrono::steady_clock) from how to access it (always @c Clock<Tag>::GetInstance()).
/// Domain selection is compile-time: a component depending on @c Clock<HighResSteadyTime> cannot
/// accidentally read @c VehicleTime at runtime.
///
/// All clock domains share the same API surface:
///   - @c Now()                  — snapshot (time_point + optional quality status)
///   - @c Subscribe<E>()         — async event subscription (where supported)
///   - @c Unsubscribe<E>()       — remove subscription
///   - @c IsAvailable()          — non-blocking readiness check
///   - @c WaitUntilAvailable()   — blocking readiness wait with stop_token
///   - @c GetInstance()          — process-wide singleton factory
///
/// @c Clock<Tag> is cheaply copyable: copying bumps the shared_ptr ref-count so
/// both copies share the same backend. Move transfers exclusive ownership.
///
/// Thread safety: @c GetInstance() is thread-safe (mutex-protected). Instance state is immutable
/// after construction; @c Now() delegates to backend (thread safety depends on backend).
///
/// @tparam Tag Clock domain tag struct (e.g. VehicleTime, HighResSteadyTime, std::chrono::steady_clock).
template <typename Tag>
class Clock
{
    using Trait = ClockTraits<Tag>;
    using Backend = typename Trait::Backend;

  public:
    using duration = typename Trait::Duration;
    using time_point = typename Trait::Timepoint;

    /// @brief Snapshot type returned by @c Now().
    using Snapshot = typename Trait::Snapshot;

    /// @brief Returns the process-wide @c Clock<Tag> handle.
    ///
    /// Acquires @c instance_guard_ mutex to protect static state. Returns override if active,
    /// cached instance if alive, or creates fresh backend via @c detail::CreateBackend<Tag>().
    [[nodiscard]] static Clock GetInstance() noexcept
    {
        std::lock_guard<std::mutex> lock{instance_guard_};
        if (instance_override_)
        {
            return Clock{instance_override_};
        }
        if (auto shared = instance_cache_.lock())
        {
            return Clock{shared};
        }
        auto fresh = detail::CreateBackend<Tag>();
        instance_cache_ = fresh;
        return Clock{fresh};
    }

    /// @brief Returns the current clock snapshot (time_point + optional status).
    ///
    /// Delegates to backend via @c Trait::CallNow(). Synchronous; thread safety depends on backend.
    [[nodiscard]] Snapshot Now() const noexcept
    {
        return Trait::CallNow(*impl_);
    }

    /// @brief Subscribes to a clock event of type @p EventType.
    ///
    /// A @c SubscriptionHook<Tag, EventType> specialization must exist; otherwise
    /// this call is a compile error (incomplete type). Callback is moved to backend.
    /// Thread safety depends on hook implementation for Tag/EventType pair.
    ///
    /// @tparam EventType  The event struct type.
    /// @param  cb         Callback invoked on each event.
    template <typename EventType>
    void Subscribe(typename SubscriptionHook<Tag, EventType>::Callback cb) noexcept
    {
        SubscriptionHook<Tag, EventType>::Subscribe(*impl_, std::move(cb));
    }

    /// @brief Removes the subscription for @p EventType.
    template <typename EventType>
    void Unsubscribe() noexcept
    {
        SubscriptionHook<Tag, EventType>::Unsubscribe(*impl_);
    }

    /// @brief Initializes the clock backend resource.
    ///
    /// Must be called once by the handle owner before reading time. Until @c Init() returns
    /// @c true, @c Now() returns a snapshot with @c kUnknown status. If @c Init() returns
    /// @c false, the call may be retried. Once succeeded, subsequent calls return @c true
    /// immediately.
    ///
    /// Only available for clock domains that require explicit initialisation (e.g. VehicleTime).
    /// Calling this on an always-ready clock (HighResSteadyTime, steady_clock) is a compile error.
    ///
    /// @return @c true if the backend is ready; @c false on failure.
    template <typename T = Tag, std::enable_if_t<HasInitialization<T>::value, bool> = true>
    [[nodiscard]] bool Init() noexcept
    {
        return InitializationHook<T>::CallInit(*impl_);
    }

    /// @brief Returns @c true if the clock backend resource is ready.
    ///
    /// Only available for clock domains that require a readiness check (e.g. VehicleTime).
    /// Calling this on an always-available clock (HighResSteadyTime, steady_clock) is a compile error.
    template <typename T = Tag, std::enable_if_t<HasAvailability<T>::value, int> = 0>
    [[nodiscard]] bool IsAvailable() const noexcept
    {
        return AvailabilityHook<T>::CallIsAvailable(*impl_);
    }

    /// @brief Blocks until the clock resource is available or the stop-token / deadline fires.
    ///
    /// Only available for clock domains that require a readiness check (e.g. VehicleTime).
    /// Calling this on an always-available clock (HighResSteadyTime, steady_clock) is a compile error.
    ///
    /// @param token  Stop token that can interrupt the wait from outside.
    /// @param until  Steady-clock deadline after which the wait is abandoned.
    ///
    /// @return @c true if the resource became available before the deadline.
    template <typename T = Tag, std::enable_if_t<HasAvailability<T>::value, int> = 0>
    [[nodiscard]] bool WaitUntilAvailable(const score::cpp::stop_token& token,
                                          std::chrono::steady_clock::time_point until) const noexcept
    {
        return AvailabilityHook<T>::CallWaitUntilAvailable(*impl_, token, until);
    }

  private:
    friend class test_utils::ScopedClockOverride<Tag>;
    friend class test_utils::ClockTestFactory<Tag>;

    /// @brief Installs a test-double backend for this @c Tag.
    ///
    /// Called only by @c ScopedClockOverride<Tag> constructor.
    static void OverrideForTest(std::shared_ptr<Backend> impl) noexcept
    {
        std::lock_guard<std::mutex> lock{instance_guard_};
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            !instance_override_,
            "score::time: Clock::OverrideForTest() called while an override is already active. "
            "Nesting ScopedClockOverride<Tag> for the same Tag is not allowed.");
        instance_override_ = std::move(impl);
    }

    /// @brief Removes the test-double backend for this @c Tag.
    ///
    /// Called only by @c ScopedClockOverride<Tag> destructor.
    static void ResetOverride() noexcept
    {
        std::lock_guard<std::mutex> lock{instance_guard_};
        instance_override_.reset();
    }

    /// @brief Private constructor called only by @c GetInstance().
    ///
    /// Takes ownership of @p impl (moved).
    explicit Clock(std::shared_ptr<Backend> impl) noexcept : impl_{std::move(impl)} {}

    /// Shared backend handle. Copying @c Clock<Tag> shares the same backend instance.
    std::shared_ptr<Backend> impl_;

    inline static std::mutex instance_guard_{};
    inline static std::weak_ptr<Backend> instance_cache_{};
    inline static std::shared_ptr<Backend> instance_override_{};
};

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_CLOCK_SRC_CLOCK_H
