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
#ifndef SCORE_TIME_DAEMON_SRC_CONTROL_FLOW_DIVIDER_CORE_CONTROL_FLOW_DIVIDER_H
#define SCORE_TIME_DAEMON_SRC_CONTROL_FLOW_DIVIDER_CORE_CONTROL_FLOW_DIVIDER_H

#include "score/time_daemon/src/common/data_flow/consumer.h"
#include "score/time_daemon/src/common/data_flow/producer.h"
#include "score/time_daemon/src/common/logging_contexts.h"
#include "score/time_daemon/src/common/machines/event_driven_machine.h"

#include "score/mw/log/logging.h"

#include <score/circular_buffer.hpp>

#include <chrono>
#include <functional>
#include <mutex>
#include <string>

namespace score
{
namespace td
{

/// @brief Component responsible for decoupling data flow and timing within VehicleTimeDaemon.
///
/// The ControlFlowDivider acts as a buffer between data producers and consumers, ensuring
/// that timing-sensitive operations are not affected by blocking operations in downstream
/// components. It maintains consistent data publishing rates by using a dedicated thread
/// and internal buffering. When new data arrives, it processes all available data immediately.
/// If no new data arrives within the specified timeout, it republishes the last known data
/// to maintain consistent output timing.
///
/// Thread safety: @c OnMessage() called from MessageBroker thread; @c OnEvent() called from
/// worker thread; @c OnTimeout() called from worker thread (accesses @c last_data_ without lock,
/// safe because only worker thread writes it). @c data_buffer_ synchronized via @c data_buffer_mutex_.
///
/// @tparam DataType  The type of data being processed (e.g. PtpTimeInfo).
/// @tparam BufferSize The size of the internal circular buffer for incoming data.
template <typename DataType, size_t BufferSize>
class ControlFlowDivider final : public EventDrivenMachine, public Consumer<DataType>, public Producer<DataType>
{
  public:
    /// @brief Constructs a ControlFlowDivider with the specified timeout.
    ///
    /// @param name    The name of this control flow divider instance.
    /// @param timeout Maximum time to wait for new data before republishing the last known data.
    explicit ControlFlowDivider(const std::string& name, std::chrono::milliseconds timeout);

    ~ControlFlowDivider() override = default;

    ControlFlowDivider(const ControlFlowDivider&) = delete;
    ControlFlowDivider& operator=(const ControlFlowDivider&) = delete;
    ControlFlowDivider(ControlFlowDivider&&) = delete;
    ControlFlowDivider& operator=(ControlFlowDivider&&) = delete;

    /// @brief Initialise the machine. Stubbed — returns true immediately as no
    /// explicit initialisation actions are required.
    bool Init() override;

    /// @brief Sets the callback function to be invoked when publishing data.
    ///
    /// @param callback Function to be called when data is published.
    void SetPublishCallback(std::function<void(const DataType&)> callback) override;

    /// @brief Receives new data and queues it for asynchronous processing.
    ///
    /// This method receives incoming data, stores it in the internal buffer,
    /// and triggers the event-driven processing mechanism to handle the data.
    /// The operation is thread-safe and non-blocking.
    ///
    /// @param data The data to be queued for processing.
    void OnMessage(DataType data) override;

  protected:
    /// @brief Drains the circular buffer and publishes each queued item in FIFO order.
    ///
    /// Called by the EventDrivenMachine worker thread when @c OnMessage() triggers
    /// an event. Acquires @c data_buffer_mutex_ and publishes all pending items.
    void OnEvent() noexcept override;

    /// @brief Republishes the last known data when no new data has arrived within the timeout.
    ///
    /// Called by the EventDrivenMachine worker thread on timeout expiry. Publishes
    /// the cached @c last_data_ to preserve a consistent output cadence even when
    /// the upstream data flow stalls.
    void OnTimeout() noexcept override;

  private:
    /// @brief Publishes data to the MessageBroker via the registered callback.
    ///
    /// @param data The data to be published.
    void Publish(const DataType& data) override;

    std::mutex data_buffer_mutex_;
    score::cpp::circular_buffer<DataType, BufferSize> data_buffer_;

    /// @brief Callback function for publishing data
    std::function<void(const DataType&)> publish_callback_;

    DataType last_data_;
};

template <typename DataType, size_t BufferSize>
ControlFlowDivider<DataType, BufferSize>::ControlFlowDivider(const std::string& name, std::chrono::milliseconds timeout)
    : EventDrivenMachine(name, timeout),
      Consumer<DataType>(),
      Producer<DataType>(),
      data_buffer_mutex_{},
      data_buffer_{},
      publish_callback_{nullptr},
      last_data_{}
{
    score::mw::log::LogInfo(kControlFlowDividerContext)
        << "ControlFlowDivider created with timeout: " << timeout.count() << "ms";
}

template <typename DataType, size_t BufferSize>
bool ControlFlowDivider<DataType, BufferSize>::Init()
{
    return true;
}

template <typename DataType, size_t BufferSize>
void ControlFlowDivider<DataType, BufferSize>::SetPublishCallback(std::function<void(const DataType&)> callback)
{
    publish_callback_ = std::move(callback);
    score::mw::log::LogDebug(kControlFlowDividerContext) << "Publish callback registered";
}

template <typename DataType, size_t BufferSize>
void ControlFlowDivider<DataType, BufferSize>::OnMessage(DataType data)
{
    score::mw::log::LogVerbose(kControlFlowDividerContext) << "New Data: " << data;
    {
        std::lock_guard<std::mutex> lock(data_buffer_mutex_);
        data_buffer_.push_back(data);
        score::mw::log::LogDebug(kControlFlowDividerContext) << "New Data queued, queue size: " << data_buffer_.size();
    }

    this->NotifyEvent();
}

template <typename DataType, size_t BufferSize>
void ControlFlowDivider<DataType, BufferSize>::OnEvent() noexcept
{
    DataType data_to_publish{};

    // because we are in OnEvent, lets assume there is data available
    bool is_data_available{true};
    while (is_data_available)
    {
        bool need_to_publish{false};
        {
            std::unique_lock<std::mutex> lock(data_buffer_mutex_);
            if (!data_buffer_.empty())
            {
                data_to_publish = data_buffer_.front();
                data_buffer_.pop_front();
                last_data_ = data_to_publish;
                need_to_publish = true;
            }
            is_data_available = !data_buffer_.empty();
        }

        if (need_to_publish)
        {
            this->Publish(data_to_publish);
        }
    }
}

template <typename DataType, size_t BufferSize>
void ControlFlowDivider<DataType, BufferSize>::OnTimeout() noexcept
{
    score::mw::log::LogWarn(kControlFlowDividerContext) << "Timeout expired, publishing last data";

    this->Publish(last_data_);
}

template <typename DataType, size_t BufferSize>
void ControlFlowDivider<DataType, BufferSize>::Publish(const DataType& data)
{
    if (publish_callback_)
    {
        score::mw::log::LogVerbose(kControlFlowDividerContext) << "Publish data: " << data;
        publish_callback_(data);
    }
    else
    {
        score::mw::log::LogError(kControlFlowDividerContext) << "No publish callback registered, data discarded";
    }
}

}  // namespace td
}  // namespace score

#endif  // SCORE_TIME_DAEMON_SRC_CONTROL_FLOW_DIVIDER_CORE_CONTROL_FLOW_DIVIDER_H
