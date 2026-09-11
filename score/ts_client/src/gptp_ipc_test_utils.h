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
#ifndef SCORE_TS_CLIENT_SRC_GPTP_IPC_TEST_UTILS_H
#define SCORE_TS_CLIENT_SRC_GPTP_IPC_TEST_UTILS_H

#include "score/memory/shared/shared_memory_factory.h"
#include "score/ts_client/src/gptp_ipc_channel.h"

#include <unistd.h>
#include <atomic>
#include <memory>
#include <string>

namespace score
{
namespace ts
{
namespace details
{

/// @brief Creates process-local unique shared-memory name for tests.
///
/// Name format: @c /gptp_ipc_ut_<pid>_<counter>.
inline std::string UniqueShmName()
{
    static std::atomic<int> counter{0};
    return "/gptp_ipc_ut_" + std::to_string(::getpid()) + "_" +
           std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

/// @brief RAII helper for manual shared-memory setup in tests.
///
/// Creates SHM via @c SharedMemoryFactory with @c GptpIpcRegion layout so
/// @c GptpIpcReceiver can open same region. Exposes raw region pointer for
/// edge-case tests that intentionally corrupt seqlock state or magic.
struct ManualShm
{
    std::shared_ptr<score::memory::shared::ISharedMemoryResource> resource_;
    GptpIpcRegion* region_{nullptr};
    std::string name_;

    /// @brief Creates shared memory region with provided name.
    /// @param n POSIX shared-memory object name.
    explicit ManualShm(const std::string& n) : name_{n}
    {
        score::memory::shared::SharedMemoryFactory::Remove(n);
        score::memory::shared::SharedMemoryFactory::RemoveStaleArtefacts(n);
        resource_ = score::memory::shared::SharedMemoryFactory::Create(
            n,
            [this](std::shared_ptr<score::memory::shared::ISharedMemoryResource> res) {
                region_ = res->construct<GptpIpcRegion>();
            },
            sizeof(GptpIpcRegion) + alignof(GptpIpcRegion) - 1U,
            score::memory::shared::permission::WorldWritable{});
    }

    /// @brief Releases shared memory resource and removes object.
    ~ManualShm()
    {
        resource_.reset();
        score::memory::shared::SharedMemoryFactory::Remove(name_);
    }

    /// @brief Returns true when region and resource are initialized.
    bool Valid() const
    {
        return resource_ != nullptr && region_ != nullptr;
    }

    /// @brief Returns mutable pointer to mapped IPC region.
    GptpIpcRegion* Region()
    {
        return region_;
    }
};

}  // namespace details
}  // namespace ts
}  // namespace score

#endif  // SCORE_TS_CLIENT_SRC_GPTP_IPC_TEST_UTILS_H
