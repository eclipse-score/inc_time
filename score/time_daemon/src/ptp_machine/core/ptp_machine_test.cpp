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
#include "score/time_daemon/src/ptp_machine/core/ptp_machine.h"
#include "score/time_daemon/src/ptp_machine/core/ptp_engine_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

namespace score
{
namespace td
{

using ::testing::_;
using ::testing::DoAll;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;

class PTPMachineTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        machine_ = std::make_unique<PTPMachineType>("PTPMachine", 100ms);

        machine_->SetPublishCallback([this](const PtpTimeInfo& data) {
            {
                std::lock_guard<std::mutex> lock(publish_data_guard_);
                published_data_ = data;
            }
            promise_data_published_.set_value();
        });
    }

    void TearDown() override
    {
        machine_->Stop();
        machine_.reset();
    }

    using PTPMachineType = PTPMachine<testing::FakePTPEngine>;
    std::unique_ptr<PTPMachineType> machine_;

    std::promise<void> promise_data_published_;
    PtpTimeInfo published_data_;
    std::mutex publish_data_guard_;
};

TEST_F(PTPMachineTest, Initialization)
{
    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), Initialize())
        .Times(Exactly(1))
        .WillOnce(Return(true));

    EXPECT_TRUE(machine_->Init());
    EXPECT_TRUE(machine_->GetName() == "PTPMachine");
}

TEST_F(PTPMachineTest, StartMachineTest)
{
    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), Initialize())
        .Times(Exactly(1))
        .WillOnce(Return(true));

    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), ReadPTPSnapshot(_))
        .WillRepeatedly(Return(true));

    machine_->Init();
    machine_->Start();

    auto future = promise_data_published_.get_future();
    EXPECT_TRUE(future.wait_for(std::chrono::milliseconds(300)) == std::future_status::ready);
}

TEST_F(PTPMachineTest, DataFlowTest)
{
    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), Initialize())
        .Times(Exactly(1))
        .WillOnce(Return(true));

    PtpTimeInfo expectedData{};
    PtpTimeInfo::ReferenceClock::time_point cur_local_time{std::chrono::nanoseconds{123}};
    std::chrono::nanoseconds cur_ptp_time{321};
    expectedData.local_time = cur_local_time;
    expectedData.ptp_assumed_time = cur_ptp_time;
    expectedData.status.is_synchronized = true;
    expectedData.rate_deviation = 0.;

    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), ReadPTPSnapshot(_))
        .WillRepeatedly(DoAll(Invoke([&expectedData](PtpTimeInfo& data) {
                                  data = expectedData;
                              }),
                              Return(true)));

    machine_->Init();
    machine_->Start();

    auto future = promise_data_published_.get_future();
    EXPECT_TRUE(future.wait_for(std::chrono::milliseconds(300)) == std::future_status::ready);

    {
        std::lock_guard<std::mutex> lock(publish_data_guard_);
        EXPECT_EQ(expectedData, published_data_);
    }
}

TEST_F(PTPMachineTest, PeriodicTaskSkipsSnapshotReadWhenNotInitialized)
{
    // Asserts behavior only; the Fatal log this guard also emits isn't checked here.
    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), ReadPTPSnapshot(_)).Times(0);

    machine_->Start();

    auto future = promise_data_published_.get_future();
    EXPECT_TRUE(future.wait_for(std::chrono::milliseconds(150)) == std::future_status::timeout);
}

TEST_F(PTPMachineTest, PeriodicTaskDoesNotPublishWhenSnapshotReadFails)
{
    // Same caveat: asserts no-publish, not the Warn log.
    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), Initialize())
        .Times(Exactly(1))
        .WillOnce(Return(true));

    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), ReadPTPSnapshot(_))
        .WillRepeatedly(Return(false));

    machine_->Init();
    machine_->Start();

    auto future = promise_data_published_.get_future();
    EXPECT_TRUE(future.wait_for(std::chrono::milliseconds(150)) == std::future_status::timeout);
}

TEST(PTPMachineStandaloneTest, PublishWithoutCallbackDoesNotCrash)
{
    PTPMachine<testing::FakePTPEngine> machine("PTPMachineNoCallback", 100ms);

    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), Initialize())
        .Times(Exactly(1))
        .WillOnce(Return(true));

    EXPECT_CALL(*testing::PTPEngineMockProvider::GetInstance().GetMock(), ReadPTPSnapshot(_))
        .Times(::testing::AtLeast(1))
        .WillRepeatedly(Return(true));

    machine.Init();
    machine.Start();

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    machine.Stop();
}

}  // namespace td
}  // namespace score
