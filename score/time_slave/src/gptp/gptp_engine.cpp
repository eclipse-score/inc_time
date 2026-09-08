/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
#include "score/time_slave/src/gptp/gptp_engine.h"
#include "score/time_slave/src/gptp/details/clock_util.h"
#include "score/time_slave/src/gptp/details/network_identity_impl.h"
#include "score/time_slave/src/gptp/details/raw_socket_impl.h"

#include "score/mw/log/logging.h"
#include "score/time_slave/src/common/logging_contexts.h"

#include <arpa/inet.h>
#include <cstring>
#include <string_view>

namespace score
{
namespace ts
{
namespace details
{

namespace
{

constexpr int kRxTimeoutMs = 100;  // poll timeout; keeps RxLoop responsive to shutdown
constexpr int kRxBufferSize = 2048;

}  // namespace

GptpEngine::GptpEngine(GptpEngineOptions opts) noexcept
    : opts_{std::move(opts)},
      socket_{std::make_unique<RawSocketImpl>()},
      identity_{std::make_unique<NetworkIdentityImpl>()},
      codec_{},
      parser_{},
      sync_sm_{opts_.jump_future_threshold_ns},
      pdelay_{nullptr},
      phc_{opts_.phc_config}
{
}

GptpEngine::GptpEngine(GptpEngineOptions opts,
                       std::unique_ptr<RawSocket> socket,
                       std::unique_ptr<NetworkIdentity> identity) noexcept
    : opts_{std::move(opts)},
      socket_{std::move(socket)},
      identity_{std::move(identity)},
      codec_{},
      parser_{},
      sync_sm_{opts_.jump_future_threshold_ns},
      pdelay_{nullptr},
      phc_{opts_.phc_config}
{
}

GptpEngine::~GptpEngine() noexcept
{
    Deinitialize();
}

bool GptpEngine::Initialize()
{
    if (running_.load(std::memory_order_acquire))
        return true;

    if (!identity_->Resolve(opts_.iface_name))
    {
        score::mw::log::LogError(kTimeSlaveAppContext)
            << "GptpEngine: failed to resolve ClockIdentity for " << opts_.iface_name;
        return false;
    }

    pdelay_ = std::make_unique<PeerDelayMeasurer>(identity_->GetClockIdentity(), opts_.domain_number);

    if (!socket_->Open(opts_.iface_name))
    {
        score::mw::log::LogError(kTimeSlaveAppContext)
            << "GptpEngine: failed to open raw socket on " << opts_.iface_name;
        return false;
    }

    if (!socket_->EnableHwTimestamping())
    {
        score::mw::log::LogWarn(kTimeSlaveAppContext)
            << "GptpEngine: HW timestamping not available on " << opts_.iface_name << ", falling back to SW timestamps";
    }

    running_.store(true, std::memory_order_release);

    try
    {
        // std::thread constructor throws std::system_error if the OS cannot
        // create a new thread (e.g. EAGAIN — thread limit reached).
        rx_thread_ = std::thread([this]() noexcept {
            RxLoop();
        });
    }
    catch (const std::system_error& e)
    {
        score::mw::log::LogError(kTimeSlaveAppContext)
            << "GptpEngine: failed to create RxThread: " << std::string_view{e.what()};
        running_.store(false, std::memory_order_release);
        socket_->Close();
        return false;
    }

    try
    {
        pdelay_thread_ = std::thread([this]() noexcept {
            PdelayLoop();
        });
    }
    catch (const std::system_error& e)
    {
        score::mw::log::LogError(kTimeSlaveAppContext)
            << "GptpEngine: failed to create PdelayThread: " << std::string_view{e.what()};
        Deinitialize();
        return false;
    }

    score::mw::log::LogInfo(kTimeSlaveAppContext) << "GptpEngine initialized on " << opts_.iface_name;
    return true;
}

bool GptpEngine::Deinitialize()
{
    running_.store(false, std::memory_order_release);

    // Close the socket first so that the RxThread's poll() unblocks.
    socket_->Close();

    if (rx_thread_.joinable())
        rx_thread_.join();
    if (pdelay_thread_.joinable())
        pdelay_thread_.join();

    score::mw::log::LogInfo(kTimeSlaveAppContext) << "GptpEngine deinitialized";
    return true;
}

void GptpEngine::FinalizeSnapshot() noexcept
{
    if (!running_.load(std::memory_order_acquire))
        return;

    const std::int64_t mono_now = MonoNs();
    const std::int64_t timeout_ns = static_cast<std::int64_t>(opts_.sync_timeout_ms) * 1'000'000LL;

    std::lock_guard<std::mutex> lk(snapshot_mutex_);
    const bool timed_out = sync_sm_.IsTimeout(mono_now, timeout_ns);
    current_snapshot_ = pending_snapshot_;
    if (timed_out)
    {
        current_snapshot_.status.is_synchronized = false;
        current_snapshot_.status.is_timeout = true;
        current_snapshot_.status.is_correct = false;
    }
}

bool GptpEngine::ReadPTPSnapshot(score::ts::GptpIpcData& data) const noexcept
{
    if (!running_.load(std::memory_order_acquire))
        return false;

    std::lock_guard<std::mutex> lk(snapshot_mutex_);
    data = current_snapshot_;
    return true;
}

void GptpEngine::RxLoop() noexcept
{
    std::uint8_t buf[kRxBufferSize];
    ::timespec hwts{};

    while (running_.load(std::memory_order_acquire))
    {
        std::memset(&hwts, 0, sizeof(hwts));
        const int n = socket_->Recv(buf, sizeof(buf), hwts, kRxTimeoutMs);
        if (n <= 0)
            continue;
        HandlePacket(buf, n, hwts);
    }
}

void GptpEngine::PdelayLoop() noexcept
{
    ::timespec next{};
    if (::clock_gettime(CLOCK_MONOTONIC, &next) != 0)
    {
        score::mw::log::LogError(kGPtpMachineContext)
            << "GptpEngine: clock_gettime failed in PdelayLoop, thread exiting";
        return;
    }
    // Configurable warm-up before first Pdelay_Req (default 2 s)
    const std::int64_t warmup_ns = static_cast<std::int64_t>(opts_.pdelay_warmup_ms) * 1'000'000LL;
    const std::int64_t next_warmup_ns =
        static_cast<std::int64_t>(next.tv_sec) * 1'000'000'000LL + next.tv_nsec + warmup_ns;
    next.tv_sec = static_cast<time_t>(next_warmup_ns / 1'000'000'000LL);
    next.tv_nsec = static_cast<long>(next_warmup_ns % 1'000'000'000LL);

    const std::int64_t interval_ns =
        static_cast<std::int64_t>(opts_.pdelay_req_interval_ms > 0 ? opts_.pdelay_req_interval_ms : 1000) * 1'000'000LL;

    while (running_.load(std::memory_order_acquire))
    {
        const std::int64_t target_ns = static_cast<std::int64_t>(next.tv_sec) * 1'000'000'000LL + next.tv_nsec;

        while (running_.load(std::memory_order_acquire))
        {
            const std::int64_t remaining = target_ns - MonoNs();
            if (remaining <= 0)
                break;
            constexpr std::int64_t kSliceNs = 50'000'000LL;
            const std::int64_t sleep_ns = remaining < kSliceNs ? remaining : kSliceNs;
            const ::timespec slice{0, static_cast<long>(sleep_ns)};
            ::clock_nanosleep(CLOCK_MONOTONIC, 0, &slice, nullptr);
        }

        if (!running_.load(std::memory_order_acquire))
            break;

        if (pdelay_)
        {
            (void)pdelay_->SendRequest(*socket_);
        }

        const std::int64_t next_ns = target_ns + interval_ns;
        next.tv_sec = static_cast<time_t>(next_ns / 1'000'000'000LL);
        next.tv_nsec = static_cast<long>(next_ns % 1'000'000'000LL);
    }
}

void GptpEngine::HandlePacket(const std::uint8_t* frame, int len, const ::timespec& hwts) noexcept
{
    int ptp_offset = 0;
    if (!codec_.ParseEthernetHeader(frame, len, ptp_offset))
        return;

    const auto* payload = frame + ptp_offset;
    const std::size_t payload_len = static_cast<std::size_t>(len - ptp_offset);

    PTPMessage msg{};
    if (!parser_.Parse(payload, payload_len, msg))
        return;

    const TmvT hw_ts{static_cast<std::int64_t>(hwts.tv_sec) * 1'000'000'000LL + hwts.tv_nsec};

    switch (msg.msgtype)
    {
        case kPtpMsgtypePdelayReq:
            if (msg.ptpHdr.domainNumber == opts_.domain_number)
                SendPDelayResponseAndFollowUp(msg, hw_ts);
            break;

        case kPtpMsgtypeSync:
            if (msg.ptpHdr.domainNumber != opts_.domain_number)
                break;
            msg.recvHardwareTS = hw_ts;
            msg.recvMonoNs = MonoNs();
            sync_sm_.OnSync(msg);
            break;

        case kPtpMsgtypeFollowUp:
            if (msg.ptpHdr.domainNumber != opts_.domain_number)
                break;
            msg.parseMessageTs = TimestampToTmv(msg.follow_up.preciseOriginTimestamp);
            {
                auto result = sync_sm_.OnFollowUp(msg);
                if (result.has_value() && pdelay_)
                {
                    const PDelayResult pdr = pdelay_->GetResult();
                    // IEEE 802.1AS: subtract peer link delay from offset
                    if (pdr.valid)
                    {
                        result->offset_ns -= pdr.path_delay_ns;
                        result->sync_fup_data.pdelay = static_cast<std::uint64_t>(pdr.path_delay_ns);
                    }
                    else
                    {
                        result->sync_fup_data.pdelay = 0U;
                    }
                    UpdateSnapshot(*result, pdr);
                }
            }
            break;

        case kPtpMsgtypePdelayResp:
            msg.recvHardwareTS = hw_ts;
            msg.parseMessageTs = TimestampToTmv(msg.pdelay_resp.requestReceiptTimestamp);
            if (pdelay_)
                pdelay_->OnResponse(msg);
            break;

        case kPtpMsgtypePdelayRespFollowUp:
            msg.parseMessageTs = TimestampToTmv(msg.pdelay_resp_fup.responseOriginReceiptTimestamp);
            if (pdelay_)
                pdelay_->OnResponseFollowUp(msg);
            break;

        default:
            break;
    }
}

void GptpEngine::UpdateSnapshot(const SyncResult& sync, const PDelayResult& pdelay) noexcept
{
    const double rate_ratio = sync_sm_.GetNeighborRateRatio();

    {
        std::lock_guard<std::mutex> lk(snapshot_mutex_);

        const std::int64_t local_rx_ns = static_cast<std::int64_t>(sync.sync_fup_data.reference_local_timestamp);
        pending_snapshot_.ptp_assumed_time = std::chrono::nanoseconds{local_rx_ns - sync.offset_ns};
        pending_snapshot_.local_time = std::chrono::nanoseconds{sync.sync_mono_ns};
        pending_snapshot_.rate_deviation = rate_ratio;

        pending_snapshot_.status.is_synchronized = true;
        pending_snapshot_.status.is_timeout = false;
        pending_snapshot_.status.is_time_jump_future = sync.is_time_jump_future;
        pending_snapshot_.status.is_time_jump_past = sync.is_time_jump_past;
        pending_snapshot_.status.is_correct = !sync.is_time_jump_future && !sync.is_time_jump_past;

        pending_snapshot_.sync_fup_data = sync.sync_fup_data;
        pending_snapshot_.pdelay_data = pdelay.pdelay_data;
    }

    if (phc_.IsEnabled())
    {
        const bool is_step = (sync.offset_ns >= opts_.phc_config.step_threshold_ns) ||
                             (sync.offset_ns <= -opts_.phc_config.step_threshold_ns);

        phc_.AdjustOffset(sync.offset_ns);
        phc_.AdjustFrequency(rate_ratio);

        if (is_step)
        {
            score::mw::log::LogInfo(kGPtpMachineContext) << "PHC step applied: offset=" << sync.offset_ns << " ns";
        }
        else
        {
            score::mw::log::LogInfo(kGPtpMachineContext) << "PHC slew: offset=" << sync.offset_ns << " ns"
                                                         << " rate_ratio=" << rate_ratio;
        }
    }
}

void GptpEngine::SendPDelayResponseAndFollowUp(const PTPMessage& req, TmvT t2) noexcept
{
    const ClockIdentity& ci = identity_->GetClockIdentity();
    const std::array<std::uint8_t, kMacAddrLen> src_mac = {ci.id[0], ci.id[1], ci.id[2], ci.id[5], ci.id[6], ci.id[7]};

    // --- PDelayResp ---
    PTPMessage resp{};
    resp.ptpHdr.tsmt = kPtpMsgtypePdelayResp | kPtpTransportSpecific;
    resp.ptpHdr.version = kPtpVersion;
    resp.ptpHdr.domainNumber = opts_.domain_number;
    resp.ptpHdr.messageLength = htons(static_cast<std::uint16_t>(sizeof(PdelayRespBody)));
    resp.ptpHdr.flagField[0] = 0x02U;  // twoStepFlag
    resp.ptpHdr.correctionField = 0;
    resp.ptpHdr.reserved2 = 0;
    resp.ptpHdr.sourcePortIdentity.clockIdentity = ci;
    resp.ptpHdr.sourcePortIdentity.portNumber = htons(0x0001U);
    resp.ptpHdr.sequenceId = htons(req.ptpHdr.sequenceId);
    resp.ptpHdr.controlField = static_cast<std::uint8_t>(ControlField::kOther);
    resp.ptpHdr.logMessageInterval = 0x7F;
    resp.pdelay_resp.requestReceiptTimestamp = TmvToTimestamp(t2);
    resp.pdelay_resp.requestingPortIdentity.clockIdentity = req.ptpHdr.sourcePortIdentity.clockIdentity;
    resp.pdelay_resp.requestingPortIdentity.portNumber = htons(req.ptpHdr.sourcePortIdentity.portNumber);

    std::uint8_t buf[2048]{};
    unsigned int len = sizeof(PdelayRespBody);
    std::memcpy(buf, &resp, len);
    if (!codec_.AddEthernetHeader(buf, len, src_mac, sizeof(buf)))
        return;

    ::timespec hwts{};
    if (socket_->Send(buf, static_cast<int>(len), hwts) <= 0)
        return;
    const TmvT t3{static_cast<std::int64_t>(hwts.tv_sec) * kNsPerSec + hwts.tv_nsec};

    // --- PDelayRespFollowUp ---
    PTPMessage fup{};
    fup.ptpHdr.tsmt = kPtpMsgtypePdelayRespFollowUp | kPtpTransportSpecific;
    fup.ptpHdr.version = kPtpVersion;
    fup.ptpHdr.domainNumber = opts_.domain_number;
    fup.ptpHdr.messageLength = htons(static_cast<std::uint16_t>(sizeof(PdelayRespFollowUpBody)));
    fup.ptpHdr.correctionField = 0;
    fup.ptpHdr.reserved2 = 0;
    fup.ptpHdr.sourcePortIdentity.clockIdentity = ci;
    fup.ptpHdr.sourcePortIdentity.portNumber = htons(0x0001U);
    fup.ptpHdr.sequenceId = htons(req.ptpHdr.sequenceId);
    fup.ptpHdr.controlField = static_cast<std::uint8_t>(ControlField::kOther);
    fup.ptpHdr.logMessageInterval = 0x7F;
    fup.pdelay_resp_fup.responseOriginReceiptTimestamp = TmvToTimestamp(t3);
    fup.pdelay_resp_fup.requestingPortIdentity.clockIdentity = req.ptpHdr.sourcePortIdentity.clockIdentity;
    fup.pdelay_resp_fup.requestingPortIdentity.portNumber = htons(req.ptpHdr.sourcePortIdentity.portNumber);

    std::uint8_t buf2[2048]{};
    unsigned int len2 = sizeof(PdelayRespFollowUpBody);
    std::memcpy(buf2, &fup, len2);
    if (!codec_.AddEthernetHeader(buf2, len2, src_mac, sizeof(buf2)))
        return;

    ::timespec hwts2{};
    (void)socket_->Send(buf2, static_cast<int>(len2), hwts2);
}

}  // namespace details
}  // namespace ts
}  // namespace score
