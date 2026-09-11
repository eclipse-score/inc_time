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

#ifndef SCORE_TIME_CLOCK_SRC_CLOCK_STATUS_H
#define SCORE_TIME_CLOCK_SRC_CLOCK_STATUS_H

#include <cstdint>
#include <sstream>
#include <type_traits>

#include <score/assert.hpp>

namespace score
{
namespace time
{

/// @brief Encapsulates the synchronisation-quality status of a timebase as a set of bit flags.
///
/// This class is the successor to @c TimeBaseStatus. It is used by timebases that express their
/// quality state as a set of named bit-positions (e.g. @c VehicleTime::StatusFlag). Timebases
/// with no quality concept (e.g. HighResSteadyTime) use @c NoStatus instead.
///
/// @tparam FlagEnumT  Scoped enum type whose enumerators are **bit positions** (not bitmasks).
///                    The underlying type must be an unsigned integer type.
/// @tparam StorageT   Unsigned integer type used as the internal bitmask storage.
///                    Must be wide enough to hold a set bit at the highest enumerator position
///                    (i.e. @c sizeof(StorageT)*8 > max_enum_value). Defaults to @c std::uint32_t,
///                    which accommodates up to 31 distinct flag positions.
template <typename FlagEnumT, typename StorageT = std::uint32_t>
class ClockStatus final
{
    static_assert(std::is_enum<FlagEnumT>::value, "FlagEnumT must be an enum");
    static_assert(std::is_unsigned<std::underlying_type_t<FlagEnumT>>::value,
                  "FlagEnumT underlying type must be unsigned");
    static_assert(std::is_unsigned<StorageT>::value, "StorageT must be an unsigned integer type");
    using PositionT = std::underlying_type_t<FlagEnumT>;  ///< Type used to hold a bit-position value.

  public:
    /// @brief Constructs a @c ClockStatus with the given set of active flags.
    ///
    /// @param flag_list  List of flag bit-positions to set as active.
    ClockStatus(const std::initializer_list<FlagEnumT> flag_list) noexcept : status_flags_{}
    {
        for (const auto flag : flag_list)
        {
            AddStatusFlagTo(status_flags_, flag);
        }
    }

    ClockStatus() = default;
    ClockStatus(const ClockStatus&) = default;
    ClockStatus(ClockStatus&&) noexcept = default;
    ClockStatus& operator=(const ClockStatus&) = default;
    ClockStatus& operator=(ClockStatus&&) noexcept = default;
    ~ClockStatus() noexcept = default;

    /// @brief Returns @c true if the given flag bit-position is set in this status.
    ///
    /// @param flag  The bit-position to test.
    /// @return @c true if the bit-position is set; @c false otherwise.
    [[nodiscard]] bool IsFlagActive(const FlagEnumT flag) const noexcept
    {
        const PositionT flag_position{static_cast<PositionT>(flag)};
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            (flag_position < (sizeof(StorageT) * 8U)),
            "score::time: IsFlagActive() argument 'flag' exceeds storage width of ClockStatus");
        return static_cast<bool>(status_flags_ & (static_cast<StorageT>(1U) << flag_position));
    }

    /// @brief Returns @c true if any of the given flag bit-positions is set in this status.
    ///
    /// @param flag_list  The bit-positions to test.
    /// @return @c true if any of the bit-positions is set; @c false otherwise.
    [[nodiscard]] bool IsAnyOfFlagsActive(const std::initializer_list<FlagEnumT>& flag_list) const noexcept
    {
        bool is_any_flag_set{false};
        for (const auto flag : flag_list)
        {
            if (IsFlagActive(flag))
            {
                is_any_flag_set = true;
                break;
            }
        }
        return is_any_flag_set;
    }

    /// @brief Formats all active flags into an @c ostringstream for diagnostics.
    ///
    /// This is a hook for domain implementors. There is **no base-template body** —
    /// each concrete @c FlagEnumT must provide an explicit full specialization, e.g.:
    ///
    /// @code{.cpp}
    /// template <>
    /// std::ostringstream ClockStatus<MyDomain::StatusFlag>::PrintTo() const
    /// {
    ///     std::ostringstream oss;
    ///     oss << (IsFlagActive(MyDomain::StatusFlag::kFoo) ? "kFoo " : "");
    ///     return oss;
    /// }
    /// @endcode
    ///
    /// @return An @c ostringstream containing the names of all active flags, separated by spaces.
    ///
    /// @note Coverage of this method lives in the domain-level test (e.g.
    ///       @c vehicle_time_status_test.cpp), not in this file.
    std::ostringstream PrintTo() const;

    /// @brief Adds a flag bit-position to this status.
    ///
    /// @param flag  The bit-position to set.
    void AddFlag(const FlagEnumT flag) noexcept
    {
        AddStatusFlagTo(status_flags_, flag);
    }

    /// @brief Compares two @c ClockStatus objects for equality.
    /// @return @c true if both objects have the same set of active flags; @c false otherwise.
    friend bool operator==(const ClockStatus& lhs, const ClockStatus& rhs) noexcept
    {
        return lhs.status_flags_ == rhs.status_flags_;
    }

    /// @brief Returns the underlying raw flag bitmask.
    constexpr StorageT ToUnderlying() const noexcept
    {
        return status_flags_;
    }

    /// @brief Replaces the internal flag bitmask with the given raw value.
    ///
    /// @param status_flags  New raw bitmask to store.
    void FromUnderlying(const StorageT status_flags) noexcept
    {
        status_flags_ = status_flags;
    }

  private:
    /// @brief Adds the bit-position represented by @p flag into @p status_container.
    static void AddStatusFlagTo(StorageT& status_container, const FlagEnumT flag) noexcept
    {
        const PositionT flag_position{static_cast<PositionT>(flag)};
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(
            (flag_position < (sizeof(StorageT) * 8U)),
            "score::time: AddStatusFlagTo() argument 'flag' exceeds storage width of ClockStatus");
        const StorageT temp{static_cast<StorageT>(1U) << flag_position};
        status_container = static_cast<StorageT>(status_container | temp);
    }

    /// @brief Raw storage for all active flag bits.
    StorageT status_flags_{};
};

/// @brief Stream insertion operator — delegates to ClockStatus::PrintTo().
template <typename OutputStream, typename FlagEnumT, typename StorageT>
auto operator<<(OutputStream& output_stream, const ClockStatus<FlagEnumT, StorageT>& clock_status) -> OutputStream&
{
    output_stream << clock_status.PrintTo().str();
    return output_stream;
}

}  // namespace time
}  // namespace score

#endif  // SCORE_TIME_CLOCK_SRC_CLOCK_STATUS_H
