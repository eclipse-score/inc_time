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
#include "score/time_daemon/src/ipc/svt/svt_time_info.h"

#include <chrono>
#include <functional>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

namespace score
{
namespace td
{

namespace
{

PtpTimeInfo MakePtpInfo()
{
    PtpTimeInfo info{};
    info.ptp_assumed_time = std::chrono::nanoseconds{1234};
    info.local_time = PtpTimeInfo::ReferenceClock::time_point{std::chrono::nanoseconds{5678}};
    info.rate_deviation = 1.0;
    info.status = {true, false, true, false, true};
    info.sync_fup_data = {11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U};
    info.pdelay_data = {21U, 22U, 23U, 24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U, 32U};
    return info;
}

svt::TimeBaseSnapshot MakeSnapshot()
{
    svt::TimeBaseSnapshot snapshot{};
    snapshot.ptp_assumed_time = 1234U;
    snapshot.local_time = 5678U;
    snapshot.rate_deviation = 1.0;
    snapshot.status = {true, false, true, false, true};
    snapshot.sync_fup_data = {11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U};
    snapshot.pdelay_data = {21U, 22U, 23U, 24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U, 32U};
    return snapshot;
}

}  // namespace

TEST(TimeBaseStatusTest, EqualsWhenAllFieldsMatch)
{
    const svt::TimeBaseStatus first{true, false, true, false, true};
    const svt::TimeBaseStatus second{true, false, true, false, true};

    EXPECT_TRUE(first == second);
}

TEST(TimeBaseStatusTest, NotEqualsWhenAnyFieldDiffers)
{
    const svt::TimeBaseStatus first{true, false, true, false, true};
    const svt::TimeBaseStatus second{false, false, true, false, true};

    EXPECT_FALSE(first == second);
}

TEST(TimeBaseStatusTest, NotEqualsWhenEachFieldDiffers)
{
    const svt::TimeBaseStatus baseline{true, false, true, false, true};

    const std::vector<std::function<void(svt::TimeBaseStatus&)>> mutations = {
        [](svt::TimeBaseStatus& value) {
            value.is_synchronized = !value.is_synchronized;
        },
        [](svt::TimeBaseStatus& value) {
            value.is_timeout = !value.is_timeout;
        },
        [](svt::TimeBaseStatus& value) {
            value.is_time_jump_future = !value.is_time_jump_future;
        },
        [](svt::TimeBaseStatus& value) {
            value.is_time_jump_past = !value.is_time_jump_past;
        },
        [](svt::TimeBaseStatus& value) {
            value.is_correct = !value.is_correct;
        },
    };

    for (const auto& mutation : mutations)
    {
        auto changed = baseline;
        mutation(changed);
        EXPECT_FALSE(baseline == changed);
    }
}

TEST(SyncFupSnapshotTest, EqualsWhenAllFieldsMatch)
{
    const svt::SyncFupSnapshot first{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    const svt::SyncFupSnapshot second{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};

    EXPECT_TRUE(first == second);
}

TEST(SyncFupSnapshotTest, NotEqualsOperatorReturnsTrueWhenDifferent)
{
    const svt::SyncFupSnapshot first{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    const svt::SyncFupSnapshot second{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 10U};

    EXPECT_TRUE(first != second);
}

TEST(SyncFupSnapshotTest, NotEqualsWhenEachFieldDiffers)
{
    const svt::SyncFupSnapshot baseline{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};

    const std::vector<std::function<void(svt::SyncFupSnapshot&)>> mutations = {
        [](svt::SyncFupSnapshot& value) {
            value.precise_origin_timestamp += 10U;
        },
        [](svt::SyncFupSnapshot& value) {
            value.reference_global_timestamp += 10U;
        },
        [](svt::SyncFupSnapshot& value) {
            value.reference_local_timestamp += 10U;
        },
        [](svt::SyncFupSnapshot& value) {
            value.sync_ingress_timestamp += 10U;
        },
        [](svt::SyncFupSnapshot& value) {
            value.correction_field += 10U;
        },
        [](svt::SyncFupSnapshot& value) {
            value.sequence_id += 1U;
        },
        [](svt::SyncFupSnapshot& value) {
            value.pdelay += 10U;
        },
        [](svt::SyncFupSnapshot& value) {
            value.port_number += 1U;
        },
        [](svt::SyncFupSnapshot& value) {
            value.clock_identity += 10U;
        },
    };

    for (const auto& mutation : mutations)
    {
        auto changed = baseline;
        mutation(changed);
        EXPECT_TRUE(baseline != changed);
    }
}

TEST(PDelayDataSnapshotTest, EqualsWhenAllFieldsMatch)
{
    const svt::PDelayDataSnapshot first{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};
    const svt::PDelayDataSnapshot second{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};

    EXPECT_TRUE(first == second);
}

TEST(PDelayDataSnapshotTest, NotEqualsOperatorReturnsTrueWhenDifferent)
{
    const svt::PDelayDataSnapshot first{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};
    const svt::PDelayDataSnapshot second{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 13U};

    EXPECT_TRUE(first != second);
}

TEST(PDelayDataSnapshotTest, NotEqualsWhenEachFieldDiffers)
{
    const svt::PDelayDataSnapshot baseline{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};

    const std::vector<std::function<void(svt::PDelayDataSnapshot&)>> mutations = {
        [](svt::PDelayDataSnapshot& value) {
            value.request_origin_timestamp += 10U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.request_receipt_timestamp += 10U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.response_origin_timestamp += 10U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.response_receipt_timestamp += 10U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.reference_global_timestamp += 10U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.reference_local_timestamp += 10U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.sequence_id += 1U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.pdelay += 10U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.req_port_number += 1U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.req_clock_identity += 10U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.resp_port_number += 1U;
        },
        [](svt::PDelayDataSnapshot& value) {
            value.resp_clock_identity += 10U;
        },
    };

    for (const auto& mutation : mutations)
    {
        auto changed = baseline;
        mutation(changed);
        EXPECT_TRUE(baseline != changed);
    }
}

TEST(TimeBaseSnapshotTest, CreateFromCopiesAndConvertsFields)
{
    PtpTimeInfo info = MakePtpInfo();

    svt::TimeBaseSnapshot snapshot{};
    snapshot.CreateFrom(info);

    EXPECT_EQ(snapshot.ptp_assumed_time, 1234U);
    EXPECT_EQ(snapshot.local_time, 5678U);
    EXPECT_DOUBLE_EQ(snapshot.rate_deviation, 1.0);
    EXPECT_TRUE((snapshot.status == svt::TimeBaseStatus{true, false, true, false, true}));
    EXPECT_TRUE((snapshot.sync_fup_data == svt::SyncFupSnapshot{11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U}));
    EXPECT_TRUE(
        (snapshot.pdelay_data == svt::PDelayDataSnapshot{21U, 22U, 23U, 24U, 25U, 26U, 27U, 28U, 29U, 30U, 31U, 32U}));
}

TEST(TimeBaseSnapshotTest, EqualsUsesToleranceForRateDeviation)
{
    svt::TimeBaseSnapshot first{};
    first.ptp_assumed_time = 1U;
    first.local_time = 2U;
    first.rate_deviation = 1.0;
    first.status = {true, false, false, false, true};
    first.sync_fup_data = {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};
    first.pdelay_data = {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};

    svt::TimeBaseSnapshot second = first;
    second.rate_deviation = 1.0 + (std::numeric_limits<double>::epsilon() / 2.0);

    EXPECT_TRUE(first == second);
}

TEST(TimeBaseSnapshotTest, NotEqualsWhenRateDeviationOutsideTolerance)
{
    svt::TimeBaseSnapshot first{};
    first.ptp_assumed_time = 1U;
    first.local_time = 2U;
    first.rate_deviation = 1.0;
    first.status = {true, false, false, false, true};
    first.sync_fup_data = {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};
    first.pdelay_data = {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};

    svt::TimeBaseSnapshot second = first;
    second.rate_deviation = 1.0 + (std::numeric_limits<double>::epsilon() * 2.0);

    EXPECT_TRUE(first != second);
}

TEST(TimeBaseSnapshotTest, EqualsPtpTimeInfoWhenAllFieldsMatch)
{
    PtpTimeInfo info = MakePtpInfo();
    svt::TimeBaseSnapshot snapshot = MakeSnapshot();

    EXPECT_TRUE(snapshot == info);
}

TEST(TimeBaseSnapshotTest, NotEqualsPtpTimeInfoWhenRateDeviationOutsideTolerance)
{
    PtpTimeInfo info = MakePtpInfo();
    svt::TimeBaseSnapshot snapshot = MakeSnapshot();
    snapshot.rate_deviation = 1.0 + (std::numeric_limits<double>::epsilon() * 2.0);

    EXPECT_TRUE(snapshot != info);
}

TEST(TimeBaseSnapshotTest, NotEqualsPtpTimeInfoWhenEachTopLevelFieldDiffers)
{
    const PtpTimeInfo info = MakePtpInfo();
    const svt::TimeBaseSnapshot baseline = MakeSnapshot();

    const std::vector<std::function<void(svt::TimeBaseSnapshot&)>> mutations = {
        [](svt::TimeBaseSnapshot& value) {
            value.local_time += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.ptp_assumed_time += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.status.is_correct = !value.status.is_correct;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.sequence_id += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.sequence_id += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.rate_deviation += std::numeric_limits<double>::epsilon() * 4.0;
        },
    };

    for (const auto& mutation : mutations)
    {
        auto changed = baseline;
        mutation(changed);
        EXPECT_TRUE(changed != info);
    }
}

TEST(TimeBaseSnapshotTest, NotEqualsPtpTimeInfoWhenAnyStatusFieldDiffers)
{
    const PtpTimeInfo info = MakePtpInfo();
    const svt::TimeBaseSnapshot baseline = MakeSnapshot();

    const std::vector<std::function<void(svt::TimeBaseSnapshot&)>> mutations = {
        [](svt::TimeBaseSnapshot& value) {
            value.status.is_correct = !value.status.is_correct;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.status.is_synchronized = !value.status.is_synchronized;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.status.is_timeout = !value.status.is_timeout;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.status.is_time_jump_future = !value.status.is_time_jump_future;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.status.is_time_jump_past = !value.status.is_time_jump_past;
        },
    };

    for (const auto& mutation : mutations)
    {
        auto changed = baseline;
        mutation(changed);
        EXPECT_TRUE(changed != info);
    }
}

TEST(TimeBaseSnapshotTest, NotEqualsPtpTimeInfoWhenAnySyncFieldDiffers)
{
    const PtpTimeInfo info = MakePtpInfo();
    const svt::TimeBaseSnapshot baseline = MakeSnapshot();

    const std::vector<std::function<void(svt::TimeBaseSnapshot&)>> mutations = {
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.clock_identity += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.correction_field += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.port_number += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.precise_origin_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.reference_global_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.reference_local_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.sequence_id += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.sync_ingress_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.pdelay += 10U;
        },
    };

    for (const auto& mutation : mutations)
    {
        auto changed = baseline;
        mutation(changed);
        EXPECT_TRUE(changed != info);
    }
}

TEST(TimeBaseSnapshotTest, NotEqualsPtpTimeInfoWhenAnyPdelayFieldDiffers)
{
    const PtpTimeInfo info = MakePtpInfo();
    const svt::TimeBaseSnapshot baseline = MakeSnapshot();

    const std::vector<std::function<void(svt::TimeBaseSnapshot&)>> mutations = {
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.req_clock_identity += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.req_port_number += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.request_origin_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.request_receipt_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.response_origin_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.response_receipt_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.reference_global_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.reference_local_timestamp += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.sequence_id += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.pdelay += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.resp_clock_identity += 10U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.resp_port_number += 1U;
        },
    };

    for (const auto& mutation : mutations)
    {
        auto changed = baseline;
        mutation(changed);
        EXPECT_TRUE(changed != info);
    }
}

TEST(TimeBaseSnapshotTest, NotEqualsWhenEachTopLevelFieldDiffers)
{
    const svt::TimeBaseSnapshot baseline = MakeSnapshot();

    const std::vector<std::function<void(svt::TimeBaseSnapshot&)>> mutations = {
        [](svt::TimeBaseSnapshot& value) {
            value.local_time += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.ptp_assumed_time += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.status.is_correct = !value.status.is_correct;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.sync_fup_data.sequence_id += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.pdelay_data.sequence_id += 1U;
        },
        [](svt::TimeBaseSnapshot& value) {
            value.rate_deviation += std::numeric_limits<double>::epsilon() * 4.0;
        },
    };

    for (const auto& mutation : mutations)
    {
        auto changed = baseline;
        mutation(changed);
        EXPECT_TRUE(baseline != changed);
    }
}

}  // namespace td
}  // namespace score
