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
#include "score/time_daemon/src/msg_broker/msg_broker.h"
#include "score/time_daemon/src/common/data_flow/consumer.h"
#include "score/time_daemon/src/common/data_flow/producer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace score
{
namespace td
{

template <typename T>
class MockConsumer : public Consumer<T>
{
  public:
    MockConsumer()
    {
        ON_CALL(*this, OnMessage(::testing::_)).WillByDefault([this](T data) {
            received_data.push_back(data);
        });
    }

    std::vector<T> received_data;

    MOCK_METHOD(void, OnMessage, (T), (override));
};

template <typename T>
class MockProducer : public Producer<T>
{
  public:
    std::function<void(const T&)> publish_callback_;

    void SetPublishCallback(std::function<void(const T&)> callback) override
    {
        publish_callback_ = std::move(callback);
    }

    void Publish(const T& data) override
    {
        if (publish_callback_)
            publish_callback_(data);
    }

    void Produce(const T& data)
    {
        Publish(data);
    }
};

class MessageBrokerTest : public ::testing::Test
{
    void SetUp() override
    {
        broker = std::make_shared<MessageBroker<int>>();
    }

  protected:
    std::shared_ptr<MessageBroker<int>> broker;
};

TEST_F(MessageBrokerTest, SingleSubscriberReceivesData)
{
    auto consumer = std::make_shared<MockConsumer<int>>();
    broker->AddSubscriber(Topic("topic1"), consumer);

    auto producer = std::make_shared<MockProducer<int>>();
    broker->AddProducer(Topic("topic1"), producer);

    producer->Produce(42);

    ASSERT_EQ(consumer->received_data.size(), 1);
    EXPECT_EQ(consumer->received_data[0], 42);
}

TEST_F(MessageBrokerTest, MultipleSubscribersReceiveData)
{
    auto consumer1 = std::make_shared<MockConsumer<int>>();
    auto consumer2 = std::make_shared<MockConsumer<int>>();

    broker->AddSubscriber(Topic("topic1"), consumer1);
    broker->AddSubscriber(Topic("topic1"), consumer2);

    auto producer = std::make_shared<MockProducer<int>>();
    broker->AddProducer(Topic("topic1"), producer);

    producer->Produce(100);

    EXPECT_EQ(consumer1->received_data.size(), 1);
    EXPECT_EQ(consumer2->received_data.size(), 1);
    EXPECT_EQ(consumer1->received_data[0], 100);
    EXPECT_EQ(consumer2->received_data[0], 100);
}

TEST_F(MessageBrokerTest, NoSubscriberForOtherTopic)
{
    auto consumer = std::make_shared<MockConsumer<int>>();
    broker->AddSubscriber(Topic("topic1"), consumer);

    auto producer = std::make_shared<MockProducer<int>>();
    broker->AddProducer(Topic("topic2"), producer);

    producer->Produce(7);

    EXPECT_TRUE(consumer->received_data.empty());
}

TEST_F(MessageBrokerTest, MultipleDataProduction)
{
    auto consumer = std::make_shared<MockConsumer<int>>();
    broker->AddSubscriber(Topic("topic1"), consumer);

    auto producer = std::make_shared<MockProducer<int>>();
    broker->AddProducer(Topic("topic1"), producer);

    for (int i = 0; i < 5; ++i)
        producer->Produce(i);

    ASSERT_EQ(consumer->received_data.size(), 5);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(consumer->received_data[i], i);
}

TEST_F(MessageBrokerTest, ExpiredSubscriberDoesNotReceiveData)
{
    // weak_ptr stored by AddSubscriber; reset() simulates the subscriber already being destroyed.
    auto consumer = std::make_shared<MockConsumer<int>>();
    std::weak_ptr<MockConsumer<int>> weak_consumer = consumer;
    EXPECT_CALL(*consumer, OnMessage(::testing::_)).Times(0);
    broker->AddSubscriber(Topic("topic1"), consumer);
    consumer.reset();

    ASSERT_TRUE(weak_consumer.expired());

    auto producer = std::make_shared<MockProducer<int>>();
    broker->AddProducer(Topic("topic1"), producer);

    EXPECT_NO_THROW(producer->Produce(42));
}

TEST_F(MessageBrokerTest, ExpiredProducerNeverRegistersPublishCallback)
{
    // AddProducer() registers a callback ON the producer (SetPublishCallback); expired must skip it.
    auto producer = std::make_shared<MockProducer<int>>();
    std::weak_ptr<MockProducer<int>> weak_producer = producer;
    producer.reset();

    ASSERT_TRUE(weak_producer.expired());
    EXPECT_NO_THROW(broker->AddProducer(Topic("topic1"), weak_producer));
}

TEST_F(MessageBrokerTest, ExpiredBrokerDoesNotProcessCallback)
{
    // Live consumer makes broker expiry observable: delivery must stop after broker.reset().
    auto consumer = std::make_shared<MockConsumer<int>>();
    broker->AddSubscriber(Topic("topic1"), consumer);

    auto producer = std::make_shared<MockProducer<int>>();
    broker->AddProducer(Topic("topic1"), producer);

    producer->Produce(1);
    ASSERT_EQ(consumer->received_data.size(), 1U);

    broker.reset();

    EXPECT_NO_THROW(producer->Produce(2));
    EXPECT_EQ(consumer->received_data.size(), 1U);
}

TEST(MessageBrokerTopicTest, LongNameIsTrimmedToMaximumLength)
{
    const std::string long_name(128U, 'x');
    Topic topic(long_name);

    EXPECT_EQ(topic.Name().size(), 32U);
    EXPECT_EQ(topic.Name(), long_name.substr(0U, 32U));
}

TEST(MessageBrokerTopicTest, TopicComparisonOperators)
{
    const Topic a("alpha");
    const Topic b("alpha");
    const Topic c("beta");

    EXPECT_TRUE(a == b);
    EXPECT_TRUE(a != c);
    EXPECT_TRUE(a < c);
}

}  // namespace td
}  // namespace score
