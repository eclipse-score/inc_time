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
#ifndef SCORE_TIME_DAEMON_SRC_IPC_CORE_PUBLISHER_IMPL_H
#define SCORE_TIME_DAEMON_SRC_IPC_CORE_PUBLISHER_IMPL_H

#include "score/time_daemon/src/common/data_flow/consumer.h"
#include "score/time_daemon/src/common/logging_contexts.h"
#include "score/time_daemon/src/common/machines/reactive_machine.h"

#include "score/time_daemon/src/ipc/core/shared_memory_handler.h"
#include "score/time_daemon/src/ipc/data_converter.h"

namespace score
{
namespace td
{

/// @brief IPC Machine publisher component that distributes verified time data
/// to client applications via shared memory.
///
/// Receives verified @c PtpTimeInfo from the MessageBroker (via the
/// @c verified_ptp_data topic), converts it to the target IPC format (e.g.
/// @c svt::TimeBaseSnapshot), and writes it to a custom shared memory channel
/// for consumption by VehicleClock backend clients.
///
/// Runs in reactive mode: @c OnMessage() is called by MessageBroker when new
/// @c verified_ptp_data arrives. @c SharedMemoryHandler::Send() is thread-safe
/// for a single writer (only one thread may call @c OnMessage() concurrently);
/// multiple readers on the application side can access the segment concurrently.
///
/// @tparam DataType    Internal data type (e.g. PtpTimeInfo from MessageBroker).
/// @tparam IpcDataType Shared memory IPC data type (e.g. svt::TimeBaseSnapshot).
///
/// @see SvtPublisher Type alias for PublisherImpl<PtpTimeInfo, svt::TimeBaseSnapshot>
/// @see SharedMemoryHandler Shared memory management implementation
/// @see ConvertToIpcData Data conversion function
template <typename DataType, typename IpcDataType>
class PublisherImpl : public ReactiveMachine, public Consumer<DataType>
{
  public:
    /// @brief Constructs the IPC publisher with a shared memory path.
    ///
    /// @param name               Publisher instance name (used for logging).
    /// @param shared_memory_path POSIX shared memory name (e.g. /td_svt_ipc).
    PublisherImpl(const std::string& name, const std::string& shared_memory_path) noexcept
        : ReactiveMachine(name), Consumer<DataType>(), shm_handler_{shared_memory_path}
    {
    }

    PublisherImpl(const PublisherImpl&) = delete;
    PublisherImpl& operator=(const PublisherImpl&) = delete;
    PublisherImpl(PublisherImpl&&) = delete;
    PublisherImpl& operator=(PublisherImpl&&) = delete;
    ~PublisherImpl() override = default;

    /// @brief Initialises the shared memory IPC channel.
    ///
    /// Creates the backing shared memory segment for downstream readers via
    /// @c SharedMemoryHandler::Init(). Must be called once before @c OnMessage().
    ///
    /// @return true if the shared memory segment was created successfully.
    bool Init() override;

    /// @brief Receives verified time data from the MessageBroker and publishes to shared memory.
    ///
    /// Converts @c DataType to @c IpcDataType via @c ConvertToIpcData(), then calls
    /// @c SharedMemoryHandler::Send() which performs a single-writer atomic write
    /// into the shared memory segment.
    ///
    /// @param data PtpTimeInfo from the @c verified_ptp_data topic.
    void OnMessage(DataType data) override;

  private:
    SharedMemoryHandler<IpcDataType> shm_handler_;
};

template <typename DataType, typename IpcDataType>
bool PublisherImpl<DataType, IpcDataType>::Init()
{
    return shm_handler_.Init();
}

template <typename DataType, typename IpcDataType>
void PublisherImpl<DataType, IpcDataType>::OnMessage(DataType data)
{
    const IpcDataType ipc_data = ConvertToIpcData<IpcDataType>(data);
    shm_handler_.Send(ipc_data);
}

}  // namespace td
}  // namespace score

#endif  // #ifndef SCORE_TIME_DAEMON_SRC_IPC_CORE_PUBLISHER_IMPL_H
