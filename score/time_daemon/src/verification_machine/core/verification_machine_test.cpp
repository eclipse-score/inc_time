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
#include "score/time_daemon/src/verification_machine/core/verification_machine.h"
#include "score/time_daemon/src/verification_machine/core/verification_stage_mock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score
{
namespace td
{

class VerificationMachineTest : public ::testing::Test
{
  protected:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(VerificationMachineTest, ControlFlowVerification)
{
    auto stage1 = std::make_unique<VerificationStageMock>();
    auto stage2 = std::make_unique<VerificationStageMock>();
    auto stage3 = std::make_unique<VerificationStageMock>();

    auto* stage_raw1 = stage1.get();
    auto* stage_raw2 = stage2.get();
    auto* stage_raw3 = stage3.get();

    VerificationMachine<ValidatorMockData> verificationMachine(
        "TestMachine",
        [&stage1]() mutable {
            return std::move(stage1);
        },
        [&stage2]() mutable {
            return std::move(stage2);
        },
        [&stage3]() mutable {
            return std::move(stage3);
        });

    {
        testing::InSequence seq;
        EXPECT_CALL(*stage_raw1, DoValidation(testing::_)).Times(1);
        EXPECT_CALL(*stage_raw2, DoValidation(testing::_)).Times(1);
        EXPECT_CALL(*stage_raw3, DoValidation(testing::_)).Times(1);
    }

    ValidatorMockData emptyData;
    verificationMachine.OnMessage(emptyData);
}

TEST_F(VerificationMachineTest, DataFlowVerification)
{
    auto stage1 = std::make_unique<VerificationStageMock>();
    auto stage2 = std::make_unique<VerificationStageMock>();
    auto stage3 = std::make_unique<VerificationStageMock>();

    auto* stage_raw1 = stage1.get();
    auto* stage_raw2 = stage2.get();
    auto* stage_raw3 = stage3.get();

    VerificationMachine<ValidatorMockData> verificationMachine(
        "TestMachine",
        [&stage1]() mutable {
            return std::move(stage1);
        },
        [&stage2]() mutable {
            return std::move(stage2);
        },
        [&stage3]() mutable {
            return std::move(stage3);
        });

    auto expectedDataStage1 = 2;
    ON_CALL(*stage_raw1, DoValidation(testing::_)).WillByDefault([expectedDataStage1](ValidatorMockData& data) {
        data.data[1] = expectedDataStage1;
    });
    auto expectedDataStage2 = 9;
    ON_CALL(*stage_raw2, DoValidation(testing::_)).WillByDefault([expectedDataStage2](ValidatorMockData& data) {
        data.data[5] = expectedDataStage2;
    });
    auto expectedDataStage3 = 20;
    ON_CALL(*stage_raw3, DoValidation(testing::_)).WillByDefault([expectedDataStage3](ValidatorMockData& data) {
        data.data[9] = expectedDataStage3;
    });

    ValidatorMockData actualData;

    // Subscribe to the publish events of the verification machine
    verificationMachine.SetPublishCallback([&actualData](const ValidatorMockData& publishedData) {
        actualData = publishedData;
    });

    ValidatorMockData data = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
    verificationMachine.OnMessage(data);

    EXPECT_EQ(actualData.data[0], data.data[0]);
    EXPECT_EQ(actualData.data[1], expectedDataStage1);
    EXPECT_EQ(actualData.data[2], data.data[2]);
    EXPECT_EQ(actualData.data[3], data.data[3]);
    EXPECT_EQ(actualData.data[4], data.data[4]);
    EXPECT_EQ(actualData.data[5], expectedDataStage2);
    EXPECT_EQ(actualData.data[6], data.data[6]);
    EXPECT_EQ(actualData.data[7], data.data[7]);
    EXPECT_EQ(actualData.data[8], data.data[8]);
    EXPECT_EQ(actualData.data[9], expectedDataStage3);
}

TEST_F(VerificationMachineTest, InitReturnsTrue)
{
    auto stage = std::make_unique<VerificationStageMock>();

    VerificationMachine<ValidatorMockData> verificationMachine("TestMachine", [&stage]() mutable {
        return std::move(stage);
    });

    EXPECT_TRUE(verificationMachine.Init());
}

class VerificationMachineDeathTest : public ::testing::Test
{
};

TEST_F(VerificationMachineDeathTest, ConstructionWithNoFactoriesAborts)
{
    ASSERT_DEATH((VerificationMachine<ValidatorMockData>("TestMachine")), "");
}

TEST_F(VerificationMachineDeathTest, FactoryReturningNullptrAborts)
{
    ASSERT_DEATH((VerificationMachine<ValidatorMockData>("TestMachine",
                                                         []() -> std::unique_ptr<VerificationStage<ValidatorMockData>> {
                                                             return nullptr;
                                                         })),
                 "");
}

}  // namespace td
}  // namespace score
