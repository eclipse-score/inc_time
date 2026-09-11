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
#include "score/time_daemon/src/ptp_machine/stub/factory.h"
#include "score/time_daemon/src/verification_machine/svt/factory.h"

#include <gtest/gtest.h>

#include <chrono>
#include <future>

namespace score
{
namespace td
{

// Verifies that PtpTimeInfo published by GPTPStubMachine flows correctly through
// SvtVerificationMachine (SynchronizationValidator -> TimeoutValidator -> TimeJumpsValidator)
// and emerges with the expected status fields intact.
class VerificationPipelineIntegrationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ptp_machine_ = CreateGPTPStubMachine("verif_pipeline_ptp");
        verifier_ = CreateSvtVerificationMachine("verif_pipeline");

        // Wire PTP machine output into the verification pipeline.
        ptp_machine_->SetPublishCallback([this](const PtpTimeInfo& data) {
            verifier_->OnMessage(data);
        });

        // Capture the first data point that exits the verification pipeline.
        verifier_->SetPublishCallback([this](const PtpTimeInfo& data) {
            try
            {
                verified_promise_.set_value(data);
            }
            catch (const std::future_error&)
            {
                // Capture only the first verified data point.
            }
        });
    }

    void TearDown() override
    {
        ptp_machine_->Stop();
    }

    std::shared_ptr<GPTPStubMachine> ptp_machine_;
    std::shared_ptr<SvtVerificationMachine> verifier_;
    std::promise<PtpTimeInfo> verified_promise_;
};

TEST_F(VerificationPipelineIntegrationTest, DataFlowsThroughPipeline)
{
    ASSERT_TRUE(verifier_->Init());
    ASSERT_TRUE(ptp_machine_->Init());
    ptp_machine_->Start();

    auto verified_future = verified_promise_.get_future();
    ASSERT_EQ(verified_future.wait_for(std::chrono::seconds(5)), std::future_status::ready);

    const auto data = verified_future.get();

    // The stub always produces synchronized/correct data on its first publish;
    // all three validators must let it through without flagging an anomaly.
    EXPECT_TRUE(data.status.is_synchronized);
    EXPECT_TRUE(data.status.is_correct);
    EXPECT_FALSE(data.status.is_timeout);
    EXPECT_FALSE(data.status.is_time_jump_future);
    EXPECT_FALSE(data.status.is_time_jump_past);
    EXPECT_GT(data.ptp_assumed_time.count(), 0);
}

}  // namespace td
}  // namespace score
