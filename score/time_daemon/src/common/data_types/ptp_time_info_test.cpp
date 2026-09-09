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
#include "score/time_daemon/src/common/data_types/ptp_time_info.h"

#include <chrono>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace score
{
namespace td
{

namespace
{

PtpTimeInfo MakePtpTimeInfoWithRateDeviation(const double rate_deviation)
{
    PtpTimeInfo info{};
    info.ptp_assumed_time = std::chrono::nanoseconds{1234};
    info.local_time = PtpTimeInfo::ReferenceClock::time_point{std::chrono::nanoseconds{5678}};
    info.rate_deviation = rate_deviation;
    info.status = {true, false, false, false, true};
    info.sync_fup_data = {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    info.pdelay_data = {11U, 12U, 13U, 14U, 15U, 16U, 17U, 18U, 19U, 20U, 21U, 22U};
    return info;
}

template <typename T>
struct FieldMutation
{
    std::string name;
    std::function<void(T&)> mutate;
};

template <typename T>
std::string FieldMutationName(const ::testing::TestParamInfo<FieldMutation<T>>& info)
{
    return info.param.name;
}

template <typename T>
void PrintTo(const FieldMutation<T>& mutation, std::ostream* os)
{
    *os << mutation.name;
}

}  // namespace

TEST(PtpStatusTest, EqualsWhenAllFieldsMatch)
{
    const PtpStatus first{true, false, true, false, true};
    const PtpStatus second{true, false, true, false, true};

    EXPECT_TRUE(first == second);
}

TEST(PtpStatusTest, NotEqualsWhenAnyFieldDiffers)
{
    const PtpStatus first{true, false, true, false, true};
    const PtpStatus second{false, false, true, false, true};

    EXPECT_FALSE(first == second);
}

class PtpStatusMutationTest : public ::testing::TestWithParam<FieldMutation<PtpStatus>>
{
};

TEST_P(PtpStatusMutationTest, ChangingFieldBreaksEquality)
{
    const PtpStatus baseline{true, false, true, false, true};

    auto changed = baseline;
    GetParam().mutate(changed);

    EXPECT_FALSE(baseline == changed);
}

INSTANTIATE_TEST_SUITE_P(AllFields,
                         PtpStatusMutationTest,
                         ::testing::Values(FieldMutation<PtpStatus>{"is_synchronized",
                                                                    [](PtpStatus& value) {
                                                                        value.is_synchronized = !value.is_synchronized;
                                                                    }},
                                           FieldMutation<PtpStatus>{"is_timeout",
                                                                    [](PtpStatus& value) {
                                                                        value.is_timeout = !value.is_timeout;
                                                                    }},
                                           FieldMutation<PtpStatus>{"is_time_jump_future",
                                                                    [](PtpStatus& value) {
                                                                        value.is_time_jump_future =
                                                                            !value.is_time_jump_future;
                                                                    }},
                                           FieldMutation<PtpStatus>{"is_time_jump_past",
                                                                    [](PtpStatus& value) {
                                                                        value.is_time_jump_past =
                                                                            !value.is_time_jump_past;
                                                                    }},
                                           FieldMutation<PtpStatus>{"is_correct",
                                                                    [](PtpStatus& value) {
                                                                        value.is_correct = !value.is_correct;
                                                                    }}),
                         FieldMutationName<PtpStatus>);

TEST(SyncFupDataTest, EqualsWhenAllFieldsMatch)
{
    const SyncFupData first{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    const SyncFupData second{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};

    EXPECT_TRUE(first == second);
}

TEST(SyncFupDataTest, NotEqualsOperatorReturnsTrueWhenDifferent)
{
    const SyncFupData first{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};
    const SyncFupData second{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 10U};

    EXPECT_TRUE(first != second);
}

class SyncFupDataMutationTest : public ::testing::TestWithParam<FieldMutation<SyncFupData>>
{
};

TEST_P(SyncFupDataMutationTest, ChangingFieldBreaksEquality)
{
    const SyncFupData baseline{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U};

    auto changed = baseline;
    GetParam().mutate(changed);

    EXPECT_TRUE(baseline != changed);
}

INSTANTIATE_TEST_SUITE_P(AllFields,
                         SyncFupDataMutationTest,
                         ::testing::Values(FieldMutation<SyncFupData>{"precise_origin_timestamp",
                                                                      [](SyncFupData& value) {
                                                                          value.precise_origin_timestamp += 10U;
                                                                      }},
                                           FieldMutation<SyncFupData>{"reference_global_timestamp",
                                                                      [](SyncFupData& value) {
                                                                          value.reference_global_timestamp += 10U;
                                                                      }},
                                           FieldMutation<SyncFupData>{"reference_local_timestamp",
                                                                      [](SyncFupData& value) {
                                                                          value.reference_local_timestamp += 10U;
                                                                      }},
                                           FieldMutation<SyncFupData>{"sync_ingress_timestamp",
                                                                      [](SyncFupData& value) {
                                                                          value.sync_ingress_timestamp += 10U;
                                                                      }},
                                           FieldMutation<SyncFupData>{"correction_field",
                                                                      [](SyncFupData& value) {
                                                                          value.correction_field += 10U;
                                                                      }},
                                           FieldMutation<SyncFupData>{"sequence_id",
                                                                      [](SyncFupData& value) {
                                                                          value.sequence_id += 1U;
                                                                      }},
                                           FieldMutation<SyncFupData>{"pdelay",
                                                                      [](SyncFupData& value) {
                                                                          value.pdelay += 10U;
                                                                      }},
                                           FieldMutation<SyncFupData>{"port_number",
                                                                      [](SyncFupData& value) {
                                                                          value.port_number += 1U;
                                                                      }},
                                           FieldMutation<SyncFupData>{"clock_identity",
                                                                      [](SyncFupData& value) {
                                                                          value.clock_identity += 10U;
                                                                      }}),
                         FieldMutationName<SyncFupData>);

TEST(PDelayDataTest, EqualsWhenAllFieldsMatch)
{
    const PDelayData first{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};
    const PDelayData second{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};

    EXPECT_TRUE(first == second);
}

TEST(PDelayDataTest, NotEqualsOperatorReturnsTrueWhenDifferent)
{
    const PDelayData first{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};
    const PDelayData second{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 13U};

    EXPECT_TRUE(first != second);
}

class PDelayDataMutationTest : public ::testing::TestWithParam<FieldMutation<PDelayData>>
{
};

TEST_P(PDelayDataMutationTest, ChangingFieldBreaksEquality)
{
    const PDelayData baseline{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U};

    auto changed = baseline;
    GetParam().mutate(changed);

    EXPECT_FALSE(baseline == changed);
    EXPECT_TRUE(baseline != changed);
}

INSTANTIATE_TEST_SUITE_P(AllFields,
                         PDelayDataMutationTest,
                         ::testing::Values(FieldMutation<PDelayData>{"request_origin_timestamp",
                                                                     [](PDelayData& value) {
                                                                         value.request_origin_timestamp += 10U;
                                                                     }},
                                           FieldMutation<PDelayData>{"request_receipt_timestamp",
                                                                     [](PDelayData& value) {
                                                                         value.request_receipt_timestamp += 10U;
                                                                     }},
                                           FieldMutation<PDelayData>{"response_origin_timestamp",
                                                                     [](PDelayData& value) {
                                                                         value.response_origin_timestamp += 10U;
                                                                     }},
                                           FieldMutation<PDelayData>{"response_receipt_timestamp",
                                                                     [](PDelayData& value) {
                                                                         value.response_receipt_timestamp += 10U;
                                                                     }},
                                           FieldMutation<PDelayData>{"reference_global_timestamp",
                                                                     [](PDelayData& value) {
                                                                         value.reference_global_timestamp += 10U;
                                                                     }},
                                           FieldMutation<PDelayData>{"reference_local_timestamp",
                                                                     [](PDelayData& value) {
                                                                         value.reference_local_timestamp += 10U;
                                                                     }},
                                           FieldMutation<PDelayData>{"sequence_id",
                                                                     [](PDelayData& value) {
                                                                         value.sequence_id += 1U;
                                                                     }},
                                           FieldMutation<PDelayData>{"pdelay",
                                                                     [](PDelayData& value) {
                                                                         value.pdelay += 10U;
                                                                     }},
                                           FieldMutation<PDelayData>{"req_port_number",
                                                                     [](PDelayData& value) {
                                                                         value.req_port_number += 1U;
                                                                     }},
                                           FieldMutation<PDelayData>{"req_clock_identity",
                                                                     [](PDelayData& value) {
                                                                         value.req_clock_identity += 10U;
                                                                     }},
                                           FieldMutation<PDelayData>{"resp_port_number",
                                                                     [](PDelayData& value) {
                                                                         value.resp_port_number += 1U;
                                                                     }},
                                           FieldMutation<PDelayData>{"resp_clock_identity",
                                                                     [](PDelayData& value) {
                                                                         value.resp_clock_identity += 10U;
                                                                     }}),
                         FieldMutationName<PDelayData>);

TEST(PtpTimeInfoTest, EqualsUsesToleranceForRateDeviation)
{
    const PtpTimeInfo first = MakePtpTimeInfoWithRateDeviation(1.0);
    const PtpTimeInfo second = MakePtpTimeInfoWithRateDeviation(1.0 + (std::numeric_limits<double>::epsilon() / 2.0));

    EXPECT_TRUE(first == second);
}

TEST(PtpTimeInfoTest, NotEqualsWhenRateDeviationOutsideTolerance)
{
    const PtpTimeInfo first = MakePtpTimeInfoWithRateDeviation(1.0);
    const PtpTimeInfo second = MakePtpTimeInfoWithRateDeviation(1.0 + (std::numeric_limits<double>::epsilon() * 2.0));

    EXPECT_TRUE(first != second);
}

TEST(PtpTimeInfoTest, EqualsWhenAllFieldsMatch)
{
    const PtpTimeInfo first = MakePtpTimeInfoWithRateDeviation(1.0);
    const PtpTimeInfo second = MakePtpTimeInfoWithRateDeviation(1.0);

    EXPECT_TRUE(first == second);
}

TEST(PtpTimeInfoTest, NotEqualsWhenStatusDiffers)
{
    PtpTimeInfo first = MakePtpTimeInfoWithRateDeviation(1.0);
    PtpTimeInfo second = MakePtpTimeInfoWithRateDeviation(1.0);
    second.status.is_timeout = !second.status.is_timeout;

    EXPECT_TRUE(first != second);
}

class PtpTimeInfoMutationTest : public ::testing::TestWithParam<FieldMutation<PtpTimeInfo>>
{
};

TEST_P(PtpTimeInfoMutationTest, ChangingFieldBreaksEquality)
{
    const PtpTimeInfo baseline = MakePtpTimeInfoWithRateDeviation(1.0);

    auto changed = baseline;
    GetParam().mutate(changed);

    EXPECT_TRUE(baseline != changed);
}

INSTANTIATE_TEST_SUITE_P(
    AllTopLevelFields,
    PtpTimeInfoMutationTest,
    ::testing::Values(FieldMutation<PtpTimeInfo>{"local_time",
                                                 [](PtpTimeInfo& value) {
                                                     value.local_time += std::chrono::nanoseconds{1};
                                                 }},
                      FieldMutation<PtpTimeInfo>{"ptp_assumed_time",
                                                 [](PtpTimeInfo& value) {
                                                     value.ptp_assumed_time += std::chrono::nanoseconds{1};
                                                 }},
                      FieldMutation<PtpTimeInfo>{"rate_deviation",
                                                 [](PtpTimeInfo& value) {
                                                     value.rate_deviation +=
                                                         std::numeric_limits<double>::epsilon() * 4.0;
                                                 }},
                      FieldMutation<PtpTimeInfo>{"status",
                                                 [](PtpTimeInfo& value) {
                                                     value.status.is_correct = !value.status.is_correct;
                                                 }},
                      FieldMutation<PtpTimeInfo>{"sync_fup_data",
                                                 [](PtpTimeInfo& value) {
                                                     value.sync_fup_data.sequence_id += 1U;
                                                 }},
                      FieldMutation<PtpTimeInfo>{"pdelay_data",
                                                 [](PtpTimeInfo& value) {
                                                     value.pdelay_data.sequence_id += 1U;
                                                 }}),
    FieldMutationName<PtpTimeInfo>);

}  // namespace td
}  // namespace score
