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
#ifndef SCORE_TIME_DAEMON_SRC_PTP_MACHINE_CORE_PTP_PTP_MACHINE_H
#define SCORE_TIME_DAEMON_SRC_PTP_MACHINE_CORE_PTP_PTP_MACHINE_H

#include "score/time_daemon/src/common/data_flow/producer.h"
#include "score/time_daemon/src/common/data_types/ptp_time_info.h"
#include "score/time_daemon/src/common/logging_contexts.h"
#include "score/time_daemon/src/common/machines/periodic_machine.h"

#include "score/mw/log/logging.h"

#include <memory>

namespace score
{
namespace td
{

/// @brief Retrieves raw PTP time data from the PTP stack and publishes it to
/// the MessageBroker.
///
/// Wraps platform-specific PTP stack communication (e.g. ptpd via devctl or
/// gPTP shared memory), making stack implementations exchangeable. Runs in a
/// dedicated thread via @c PeriodicMachine to maintain a consistent publishing
/// rate even when PTP stack communication experiences delays.
///
/// Data flow per period:
///   1. @c PeriodicTask() called at the configured update interval.
///   2. @c ReadPTPSnapshot() retrieves raw data from the PTP stack via @c engine_impl_.
///   3. Data published to MessageBroker as the @c raw_ptp_data topic.
///   4. @c ControlFlowDivider receives it and separates control flow for the pipeline.
///
/// @tparam PTPEngine Platform-specific PTP stack interface (e.g. ShmPTPEngine).
///
/// @see ControlFlowDivider For control flow separation.
/// @see PtpTimeInfo For the data structure definition.
/// @see PeriodicMachine For the threading model.
template <class PTPEngine>
class PTPMachine final : public PeriodicMachine, public Producer<PtpTimeInfo>
{
  public:
    /// @brief Constructs a PTPMachine with the specified name, update interval, and PTP engine arguments.
    ///
    /// @param name           The name of the PTPMachine instance.
    /// @param updateInterval The interval at which time data is retrieved and published.
    /// @param args           Arguments forwarded to the PTPEngine constructor.
    template <typename... PTPEngineArgs>
    explicit PTPMachine(const std::string& name, std::chrono::milliseconds updateInterval, PTPEngineArgs&&... args)
        : PeriodicMachine(name, updateInterval),
          Producer<PtpTimeInfo>(),
          publish_callback_(nullptr),
          engine_impl_(std::make_unique<PTPEngine>(std::forward<PTPEngineArgs>(args)...)),
          is_initialized_(false)
    {
        score::mw::log::LogInfo(kPtpMachineContext)
            << "PTPMachine created with update interval: " << updateInterval.count() << "ms";
    }

    ~PTPMachine() override;

    PTPMachine(const PTPMachine&) = delete;
    PTPMachine& operator=(const PTPMachine&) = delete;
    PTPMachine(PTPMachine&&) = delete;
    PTPMachine& operator=(PTPMachine&&) = delete;

    /// @brief Initialises the PTP stack and prepares the machine for operation.
    ///
    /// Attempts to establish communication with the underlying PTP engine.
    /// Must be called before starting periodic time synchronisation tasks.
    ///
    /// @return true if initialisation was successful, false otherwise.
    bool Init() override;

    /// @brief Sets the callback function to be invoked when publishing data.
    ///
    /// @param callback Function to be called when data is published.
    void SetPublishCallback(std::function<void(const PtpTimeInfo&)> callback) override;

  protected:
    /// @brief Periodically retrieves and publishes the latest PTP time data.
    ///
    /// Invoked at each update interval by the PeriodicMachine worker thread.
    /// Obtains the current time information from the PTP engine and publishes
    /// it to subscribers. On read failure or uninitialised engine, logs warning
    /// and returns. @c Deinit() called explicitly in destructor.
    void PeriodicTask() noexcept override;

  private:
    /// @brief Deinitialises the PTP stack and releases associated resources.
    ///
    /// Cleans up the connection to the underlying PTP engine and resets the internal state.
    void Deinit();

    /// @brief Publishes time information data to the registered subscribers.
    ///
    /// @param data The time information to publish.
    void Publish(const PtpTimeInfo& data) override;

    /// @brief Callback invoked when publishing data.
    std::function<void(const PtpTimeInfo&)> publish_callback_;

    std::unique_ptr<PTPEngine> engine_impl_;

    bool is_initialized_;
};

template <class PTPEngine>
PTPMachine<PTPEngine>::~PTPMachine()
{
    Deinit();
}

template <class PTPEngine>
void PTPMachine<PTPEngine>::SetPublishCallback(std::function<void(const PtpTimeInfo&)> callback)
{
    publish_callback_ = std::move(callback);
}

template <class PTPEngine>
bool PTPMachine<PTPEngine>::Init()
{
    if (!is_initialized_)
    {
        is_initialized_ = engine_impl_->Initialize();

        if (is_initialized_)
        {
            score::mw::log::LogInfo(kPtpMachineContext) << "PTP stack initialized successfully";
        }
        else
        {
            score::mw::log::LogError(kPtpMachineContext) << "PTP stack initialization failed";
        }
    }

    return is_initialized_;
}

template <class PTPEngine>
void PTPMachine<PTPEngine>::Deinit()
{
    if (is_initialized_)
    {
        std::ignore = engine_impl_->Deinitialize();
        is_initialized_ = false;
    }
}

template <class PTPEngine>
void PTPMachine<PTPEngine>::PeriodicTask() noexcept
{
    if (!is_initialized_)
    {
        score::mw::log::LogFatal(kPtpMachineContext) << "PTP stack not initialized, abort periodic task";
        return;
    }

    PtpTimeInfo current_time_info{};
    auto read_success = engine_impl_->ReadPTPSnapshot(current_time_info);

    if (read_success)
    {
        Publish(current_time_info);
    }
    else
    {
        score::mw::log::LogWarn(kPtpMachineContext) << "Failed to retrieve time data from PTP stack";
    }
}

template <class PTPEngine>
void PTPMachine<PTPEngine>::Publish(const PtpTimeInfo& data)
{
    if (publish_callback_)
    {
        score::mw::log::LogDebug(kPtpMachineContext) << "Publishing PTP data";
        publish_callback_(data);
    }
}

}  // namespace td
}  // namespace score

#endif  // SCORE_TIME_DAEMON_SRC_PTP_MACHINE_CORE_PTP_PTP_MACHINE_H
