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
#ifndef SCORE_TIME_DAEMON_SRC_MSG_BROKER_MSG_BROKER_H
#define SCORE_TIME_DAEMON_SRC_MSG_BROKER_MSG_BROKER_H

#include "score/mw/log/logging.h"
#include "score/time_daemon/src/common/data_flow/consumer.h"
#include "score/time_daemon/src/common/data_flow/producer.h"
#include "score/time_daemon/src/msg_broker/subscription.h"
#include "score/time_daemon/src/msg_broker/topic.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace score
{
namespace td
{

/// @brief Central publish-subscribe communication hub within the TimeDaemon.
///
/// Manages topics and distributes messages to interested subscribers, enabling
/// decoupled communication: components evolve independently without direct
/// dependencies on each other.
///
/// MessageBroker does not provide synchronization between publish and callback
/// invocation. Callbacks run synchronously in caller's thread; no queuing.
/// To separate control flows, use @c ControlFlowDivider.
///
/// @tparam T Message data type for this broker instance (e.g. PtpTimeInfo).
///
/// @see ControlFlowDivider For control flow separation between threads.
template <typename T>
class MessageBroker : public std::enable_shared_from_this<MessageBroker<T>>
{
  public:
    /// @brief Registers a consumer component to receive messages on the specified topic.
    ///
    /// Creates a subscription wrapping the consumer's @c OnMessage() callback and
    /// adds it to the topic registry. Uses a weak_ptr to avoid prolonging the
    /// consumer's lifetime.
    ///
    /// @pre Must be called during the initialization phase to ensure thread safety.
    ///
    /// @param topic           Topic name to subscribe to.
    /// @param subscriber_weak Weak pointer to the consumer component.
    void AddSubscriber(const Topic& topic, std::weak_ptr<Consumer<T>> subscriber_weak);

    /// @brief Registers a producer component to publish messages on the specified topic.
    ///
    /// Sets the producer's publish callback to invoke @c OnNewData() on this broker.
    /// Uses a weak_ptr to avoid prolonging the producer's lifetime.
    ///
    /// @param topic         Topic name to publish on.
    /// @param producer_weak Weak pointer to the producer component.
    void AddProducer(const Topic& topic, std::weak_ptr<Producer<T>> producer_weak);

  private:
    void Publish(const Topic& topic, const T& data) const;
    void Subscribe(const Topic& topic, const Subscription<T>& subscription);
    void OnNewData(const Topic& topic, const T& data) const;

    std::unordered_map<Topic, std::vector<Subscription<T>>> subscribers_;
};

template <typename T>
void MessageBroker<T>::AddSubscriber(const Topic& topic, std::weak_ptr<Consumer<T>> subscriber_weak)
{
    Subscribe(topic, Subscription<T>([subscriber_weak](const T& data) {
                  const auto subscriber = subscriber_weak.lock();
                  if (subscriber)
                  {
                      subscriber->OnMessage(data);
                  }
              }));
}

template <typename T>
void MessageBroker<T>::AddProducer(const Topic& topic, std::weak_ptr<Producer<T>> producer_weak)
{
    const auto producer = producer_weak.lock();
    if (producer)
    {
        std::weak_ptr<MessageBroker<T>> weak_broker = this->shared_from_this();

        producer->SetPublishCallback([weak_broker, topic](const T& data) {
            const auto broker = weak_broker.lock();
            if (broker)
            {
                broker->OnNewData(topic, data);
            }
        });
    }
}

template <typename T>
void MessageBroker<T>::Publish(const Topic& topic, const T& data) const
{
    auto it = subscribers_.find(topic);
    if (it != subscribers_.end())
    {
        for (auto& sub : it->second)
        {
            if (sub.callback_)
                sub.callback_(data);
        }
    }
}

template <typename T>
void MessageBroker<T>::Subscribe(const Topic& topic, const Subscription<T>& subscription)
{
    subscribers_[topic].push_back(subscription);
}

template <typename T>
void MessageBroker<T>::OnNewData(const Topic& topic, const T& data) const
{
    Publish(topic, data);
}

}  // namespace td
}  // namespace score

#endif  // #ifndef SCORE_TIME_DAEMON_SRC_MSG_BROKER_MSG_BROKER_H
