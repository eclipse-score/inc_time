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
#ifndef SCORE_TIME_DAEMON_SRC_IPC_CORE_RECEIVER_IMPL_H
#define SCORE_TIME_DAEMON_SRC_IPC_CORE_RECEIVER_IMPL_H

#include "score/time_daemon/src/ipc/receiver.h"

#include "score/time_daemon/src/ipc/core/shared_memory_handler.h"

namespace score
{
namespace td
{

/// @brief IPC Machine receiver component for reading verified time data from
/// the shared memory channel written by @c PublisherImpl.
///
/// Opens an existing shared memory channel and reads the latest IPC-formatted
/// time data. Used by the VehicleClock backend on the application
/// side to retrieve time information from TimeDaemon.
///
/// Thread-safe for multiple concurrent readers.
/// @c SharedMemoryHandler::Receive() returns the latest snapshot atomically.
/// The call is non-blocking and returns @c std::nullopt if no data is available.
///
/// @tparam IpcDataType Shared memory IPC data type (e.g. @c svt::TimeBaseSnapshot).
///
/// @see SvtReceiver Type alias for ReceiverImpl<svt::TimeBaseSnapshot>
/// @see PublisherImpl TimeDaemon-side publisher component
template <typename IpcDataType>
class ReceiverImpl : public Receiver<IpcDataType>
{
  public:
    /// @brief Constructs the IPC receiver with a shared memory path.
    ///
    /// @param shared_memory_path POSIX shared memory name matching the publisher (e.g. /td_svt_ipc).
    ReceiverImpl(const std::string& shared_memory_path) noexcept
        : Receiver<IpcDataType>(), shm_handler_{shared_memory_path}
    {
    }

    ReceiverImpl(const ReceiverImpl&) = delete;
    ReceiverImpl& operator=(const ReceiverImpl&) = delete;
    ReceiverImpl(ReceiverImpl&&) = delete;
    ReceiverImpl& operator=(ReceiverImpl&&) = delete;
    ~ReceiverImpl() override = default;

    /// @brief Opens the existing shared memory IPC channel created by the publisher.
    ///
    /// Connects to the already-created segment by name; does not create a new segment.
    /// Non-blocking; returns false immediately if segment not found.
    ///
    /// @return true if the shared memory segment was opened successfully.
    bool Init() noexcept override;

    /// @brief Reads the latest time data from shared memory.
    ///
    /// Non-blocking. Returns @c std::nullopt if no new or valid sample is available.
    ///
    /// @return Latest IPC data if available, @c std::nullopt otherwise.
    std::optional<IpcDataType> Receive() noexcept override;

  private:
    SharedMemoryHandler<IpcDataType> shm_handler_;
};

template <typename IpcDataType>
bool ReceiverImpl<IpcDataType>::Init() noexcept
{
    return shm_handler_.Init();
}

template <typename IpcDataType>
std::optional<IpcDataType> ReceiverImpl<IpcDataType>::Receive() noexcept
{
    return shm_handler_.Receive();
}

}  // namespace td
}  // namespace score

#endif  // #ifndef SCORE_TIME_DAEMON_SRC_IPC_CORE_RECEIVER_IMPL_H
