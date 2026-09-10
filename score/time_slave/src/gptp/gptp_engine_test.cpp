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
#include "score/time_slave/src/gptp/gptp_engine.h"
#include "score/time_slave/src/gptp/details/network_identity.h"
#include "score/time_slave/src/gptp/details/raw_socket.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace score
{
namespace ts
{
namespace details
{

namespace
{

// ── FakeSocket ────────────────────────────────────────────────────────────────

class FakeSocket final : public RawSocket
{
  public:
    void Push(std::vector<std::uint8_t> data, ::timespec hwts = {})
    {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            frames_.push_back({std::move(data), hwts});
        }
        cv_.notify_one();
    }

    void SetOpenOk(bool v)
    {
        open_ok_ = v;
    }

    bool Open(const std::string&) override
    {
        return open_ok_;
    }
    bool EnableHwTimestamping() override
    {
        return hw_ts_ok_;
    }

    void Close() override
    {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    int Recv(std::uint8_t* buf, std::size_t buf_len, ::timespec& hwts, int timeout_ms) override
    {
        std::unique_lock<std::mutex> lk(mtx_);
        const auto timeout = std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 100);
        cv_.wait_for(lk, timeout, [this] {
            return closed_ || !frames_.empty();
        });
        if (closed_)
            return -1;
        if (frames_.empty())
            return 0;
        auto& [data, ts] = frames_.front();
        const std::size_t n = std::min(data.size(), buf_len);
        std::memcpy(buf, data.data(), n);
        hwts = ts;
        frames_.pop_front();
        return static_cast<int>(n);
    }

    int Send(const void*, int len, ::timespec&) override
    {
        return len;
    }
    int GetFd() const override
    {
        return -1;
    }

    void SetHwTsOk(bool v)
    {
        hw_ts_ok_ = v;
    }

  private:
    std::deque<std::pair<std::vector<std::uint8_t>, ::timespec>> frames_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool closed_{false};
    bool hw_ts_ok_{true};
    bool open_ok_{true};
};

// ── FakeIdentity ──────────────────────────────────────────────────────────────

class FakeIdentity final : public NetworkIdentity
{
  public:
    explicit FakeIdentity(bool resolve_ok = true) : resolve_ok_{resolve_ok} {}

    bool Resolve(const std::string&) override
    {
        return resolve_ok_;
    }

    ClockIdentity GetClockIdentity() const override
    {
        ClockIdentity ci{};
        ci.id[0] = 0xAA;
        ci.id[7] = 0xBB;
        return ci;
    }

  private:
    bool resolve_ok_;
};

// ── Frame builders ────────────────────────────────────────────────────────────

// 14-byte Ethernet header with EtherType 0x88F7 (IEEE 1588)
void AppendEthHeader(std::vector<std::uint8_t>& buf)
{
    // dst: 01:80:c2:00:00:0e
    const std::uint8_t dst[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};
    // src: 02:00:00:ff:00:11
    const std::uint8_t src[6] = {0x02, 0x00, 0x00, 0xFF, 0x00, 0x11};
    buf.insert(buf.end(), dst, dst + 6);
    buf.insert(buf.end(), src, src + 6);
    buf.push_back(0x88);
    buf.push_back(0xF7);
}

// Build a 34-byte PTP header at the back of buf.
void AppendPtpHeader(std::vector<std::uint8_t>& buf,
                     std::uint8_t msgtype,
                     std::uint16_t seqId,
                     std::uint8_t ctlField = 0)
{
    const std::size_t start = buf.size();
    buf.resize(start + 34, 0);
    std::uint8_t* p = buf.data() + start;
    p[0] = static_cast<std::uint8_t>(0x10U | (msgtype & 0x0FU));  // tsmt
    p[1] = 0x02;                                                  // version
    const std::uint16_t len = htons(static_cast<std::uint16_t>(buf.size() - 14));
    std::memcpy(p + 2, &len, 2);
    const std::uint16_t seq = htons(seqId);
    std::memcpy(p + 30, &seq, 2);
    p[32] = ctlField;
}

// Append a 10-byte Timestamp body (sec_msb=0, sec_lsb, ns).
void AppendTimestamp(std::vector<std::uint8_t>& buf, std::uint32_t sec_lsb, std::uint32_t ns)
{
    const std::uint16_t msb = htons(0U);
    const std::uint32_t sl = htonl(sec_lsb);
    const std::uint32_t n = htonl(ns);
    const std::uint8_t* p;
    p = reinterpret_cast<const std::uint8_t*>(&msb);
    buf.insert(buf.end(), p, p + 2);
    p = reinterpret_cast<const std::uint8_t*>(&sl);
    buf.insert(buf.end(), p, p + 4);
    p = reinterpret_cast<const std::uint8_t*>(&n);
    buf.insert(buf.end(), p, p + 4);
}

std::vector<std::uint8_t> MakeSyncFrame(std::uint16_t seqId)
{
    std::vector<std::uint8_t> f;
    AppendEthHeader(f);
    AppendPtpHeader(f, kPtpMsgtypeSync, seqId, /*ctl=*/0);
    AppendTimestamp(f, 0, 0);  // Sync body (origin timestamp, unused)
    return f;
}

std::vector<std::uint8_t> MakeFollowUpFrame(std::uint16_t seqId, std::uint32_t sec_lsb, std::uint32_t ns)
{
    std::vector<std::uint8_t> f;
    AppendEthHeader(f);
    AppendPtpHeader(f, kPtpMsgtypeFollowUp, seqId, /*ctl=*/2);
    AppendTimestamp(f, sec_lsb, ns);
    return f;
}

std::vector<std::uint8_t> MakePdelayRespFrame(std::uint16_t seqId)
{
    std::vector<std::uint8_t> f;
    AppendEthHeader(f);
    AppendPtpHeader(f, kPtpMsgtypePdelayResp, seqId, /*ctl=*/5);
    AppendTimestamp(f, 1, 0);  // responseOriginTimestamp
    // requesting port identity (10 bytes)
    f.resize(f.size() + 10, 0);
    return f;
}

std::vector<std::uint8_t> MakePdelayRespFupFrame(std::uint16_t seqId)
{
    std::vector<std::uint8_t> f;
    AppendEthHeader(f);
    AppendPtpHeader(f, kPtpMsgtypePdelayRespFollowUp, seqId, /*ctl=*/5);
    AppendTimestamp(f, 2, 0);    // responseOriginReceiptTimestamp
    f.resize(f.size() + 10, 0);  // requesting port identity
    return f;
}

std::vector<std::uint8_t> MakeUnknownFrame()
{
    std::vector<std::uint8_t> f;
    AppendEthHeader(f);
    AppendPtpHeader(f, kPtpMsgtypePdelayReq, 0, /*ctl=*/5);
    return f;
}

// ── Test helpers ──────────────────────────────────────────────────────────────

GptpEngineOptions FastOptions()
{
    GptpEngineOptions o;
    o.iface_name = "lo";
    o.pdelay_warmup_ms = 0;         // no warmup — first Pdelay_Req fires immediately
    o.pdelay_req_interval_ms = 10;  // 10 ms cycle
    o.sync_timeout_ms = 3300;
    o.jump_future_threshold_ns = 500'000'000LL;
    return o;
}

// Wait up to @p max_ms for snapshot.status.is_synchronized to become true.
bool WaitForSync(GptpEngine& eng, int max_ms = 500)
{
    for (int i = 0; i < max_ms / 10; ++i)
    {
        score::ts::GptpIpcData data{};
        eng.FinalizeSnapshot();
        eng.ReadPTPSnapshot(data);
        if (data.status.is_synchronized)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// ── Fixtures ──────────────────────────────────────────────────────────────────

// Fixture for tests that use real socket+identity paths (no injection).
class GptpEngineTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        engine_ = std::make_unique<GptpEngine>(FastOptions());
    }

    void TearDown() override
    {
        engine_->Deinitialize();
    }

    std::unique_ptr<GptpEngine> engine_;
};

// Fixture for tests that inject FakeSocket + FakeIdentity.
class GptpEngineFakeTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        auto sock = std::make_unique<FakeSocket>();
        auto identity = std::make_unique<FakeIdentity>();
        socket_raw_ = sock.get();
        engine_ = std::make_unique<GptpEngine>(FastOptions(), std::move(sock), std::move(identity));
    }

    void TearDown() override
    {
        engine_->Deinitialize();
    }

    FakeSocket* socket_raw_{nullptr};
    std::unique_ptr<GptpEngine> engine_;
};

}  // namespace

// ── GptpEngineTest — uninitialised paths ──────────────────────────────────────

TEST_F(GptpEngineTest, Deinitialize_WhenNotInitialized_ReturnsTrue)
{
    EXPECT_TRUE(engine_->Deinitialize());
}

TEST_F(GptpEngineTest, Deinitialize_CalledTwice_BothReturnTrue)
{
    EXPECT_TRUE(engine_->Deinitialize());
    EXPECT_TRUE(engine_->Deinitialize());
}

TEST_F(GptpEngineTest, ReadPTPSnapshot_WhenNotInitialized_ReturnsFalse)
{
    score::ts::GptpIpcData data{};
    EXPECT_FALSE(engine_->ReadPTPSnapshot(data));
}

TEST_F(GptpEngineTest, ReadPTPSnapshot_InfoUnchanged_WhenNotInitialized)
{
    score::ts::GptpIpcData data{};
    data.ptp_assumed_time = std::chrono::nanoseconds{999LL};
    EXPECT_FALSE(engine_->ReadPTPSnapshot(data));
    EXPECT_EQ(data.ptp_assumed_time, std::chrono::nanoseconds{999LL});
}

// ── GptpEngineFakeTest — Initialize / Deinitialize ───────────────────────────

TEST_F(GptpEngineFakeTest, Initialize_WithFakeSocket_ReturnsTrue)
{
    EXPECT_TRUE(engine_->Initialize());
}

TEST_F(GptpEngineFakeTest, Initialize_CalledTwice_ReturnsTrueOnSecondCall)
{
    EXPECT_TRUE(engine_->Initialize());
    EXPECT_TRUE(engine_->Initialize());  // already running → returns true
}

TEST_F(GptpEngineFakeTest, Deinitialize_AfterInitialize_ReturnsTrue)
{
    ASSERT_TRUE(engine_->Initialize());
    EXPECT_TRUE(engine_->Deinitialize());
}

TEST_F(GptpEngineFakeTest, ReadPTPSnapshot_AfterInitialize_ReturnsTrue)
{
    ASSERT_TRUE(engine_->Initialize());
    engine_->FinalizeSnapshot();
    score::ts::GptpIpcData data{};
    EXPECT_TRUE(engine_->ReadPTPSnapshot(data));
}

TEST_F(GptpEngineFakeTest, ReadPTPSnapshot_NotSynchronized_BeforeAnySync)
{
    ASSERT_TRUE(engine_->Initialize());
    engine_->FinalizeSnapshot();
    score::ts::GptpIpcData data{};
    ASSERT_TRUE(engine_->ReadPTPSnapshot(data));
    EXPECT_FALSE(data.status.is_synchronized);
}

// ── GptpEngineFakeTest — identity failure ─────────────────────────────────────

TEST(GptpEngineIdentityFailTest, Initialize_IdentityResolveFails_ReturnsFalse)
{
    auto sock = std::make_unique<FakeSocket>();
    auto identity = std::make_unique<FakeIdentity>(/*resolve_ok=*/false);
    GptpEngine eng{FastOptions(), std::move(sock), std::move(identity)};
    EXPECT_FALSE(eng.Initialize());
    EXPECT_TRUE(eng.Deinitialize());
}

// ── GptpEngineFakeTest — HW timestamp unavailable (warning path) ──────────────

TEST_F(GptpEngineFakeTest, Initialize_HwTsUnavailable_StillReturnsTrue)
{
    socket_raw_->SetHwTsOk(false);
    EXPECT_TRUE(engine_->Initialize());
}

// ── GptpEngineFakeTest — Sync + FollowUp → UpdateSnapshot ────────────────────

TEST_F(GptpEngineFakeTest, HandlePacket_SyncFollowUp_SnapshotBecomesSync)
{
    ASSERT_TRUE(engine_->Initialize());

    // Send Sync then FollowUp with the same seqId.
    ::timespec hwts{};
    hwts.tv_sec = 1;
    hwts.tv_nsec = 500'000'000L;
    socket_raw_->Push(MakeSyncFrame(1U), hwts);
    socket_raw_->Push(MakeFollowUpFrame(1U, /*sec=*/2, /*ns=*/0));

    EXPECT_TRUE(WaitForSync(*engine_));
    engine_->FinalizeSnapshot();
    score::ts::GptpIpcData data{};
    ASSERT_TRUE(engine_->ReadPTPSnapshot(data));
    EXPECT_TRUE(data.status.is_synchronized);
    EXPECT_FALSE(data.status.is_timeout);
}

TEST_F(GptpEngineFakeTest, HandlePacket_MultipleSyncFup_SnapshotUpdated)
{
    ASSERT_TRUE(engine_->Initialize());

    for (std::uint16_t seq = 1U; seq <= 3U; ++seq)
    {
        socket_raw_->Push(MakeSyncFrame(seq));
        socket_raw_->Push(MakeFollowUpFrame(seq, seq, 0U));
    }

    EXPECT_TRUE(WaitForSync(*engine_));
}

// ── GptpEngineFakeTest — PdelayResp + PdelayRespFollowUp ─────────────────────

TEST_F(GptpEngineFakeTest, HandlePacket_PdelayRespSequence_DoesNotCrash)
{
    ASSERT_TRUE(engine_->Initialize());

    socket_raw_->Push(MakePdelayRespFrame(0U));
    socket_raw_->Push(MakePdelayRespFupFrame(0U));

    // Just verify no crash; sleep briefly to let the RxThread process.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// ── GptpEngineFakeTest — unknown msgtype (default branch) ────────────────────

TEST_F(GptpEngineFakeTest, HandlePacket_UnknownMsgtype_DefaultBranchNocrash)
{
    ASSERT_TRUE(engine_->Initialize());
    socket_raw_->Push(MakeUnknownFrame());
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

// ── GptpEngineFakeTest — bad Ethernet header ─────────────────────────────────

TEST_F(GptpEngineFakeTest, HandlePacket_TooShortFrame_EarlyReturn)
{
    ASSERT_TRUE(engine_->Initialize());
    socket_raw_->Push({0x01, 0x02, 0x03});  // < 14 bytes, ParseEthernetHeader returns false
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
}

// ── GptpEngineFakeTest — Sync+FUP then timeout path ──────────────────────────

TEST(GptpEngineTimeoutTest, ReadPTPSnapshot_TimeoutPath_IsTimeoutSet)
{
    // Use a very short timeout (50 ms) so we can trigger it quickly.
    GptpEngineOptions opts = FastOptions();
    opts.sync_timeout_ms = 50;

    auto sock = std::make_unique<FakeSocket>();
    auto identity = std::make_unique<FakeIdentity>();
    FakeSocket* raw_sock = sock.get();

    GptpEngine eng{opts, std::move(sock), std::move(identity)};
    ASSERT_TRUE(eng.Initialize());

    // First receive a Sync+FUP so the state machine records a timestamp.
    ::timespec hwts{};
    hwts.tv_sec = 1;
    raw_sock->Push(MakeSyncFrame(1U), hwts);
    raw_sock->Push(MakeFollowUpFrame(1U, 2U, 0U));

    // Wait for it to be processed and become synchronized.
    bool got_sync = false;
    for (int i = 0; i < 50; ++i)
    {
        score::ts::GptpIpcData tmp{};
        eng.FinalizeSnapshot();
        eng.ReadPTPSnapshot(tmp);
        if (tmp.status.is_synchronized)
        {
            got_sync = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(got_sync) << "engine never became synchronized";

    // Now wait longer than sync_timeout_ms for the timeout to trigger.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    score::ts::GptpIpcData info{};
    eng.FinalizeSnapshot();
    ASSERT_TRUE(eng.ReadPTPSnapshot(info));
    EXPECT_TRUE(info.status.is_timeout);
    EXPECT_FALSE(info.status.is_synchronized);
    EXPECT_TRUE(eng.Deinitialize());
}

// ── Non-injectable path — nonexistent interface ───────────────────────────────

TEST(GptpEngineRealSocketTest, Initialize_NonExistentInterface_ReturnsFalse)
{
    GptpEngineOptions opts;
    opts.iface_name = "nonexistent_iface_xyz";
    opts.pdelay_warmup_ms = 0;
    GptpEngine eng{opts};
    EXPECT_FALSE(eng.Initialize());
    EXPECT_TRUE(eng.Deinitialize());
}

// ── Socket-open-fail path (lines 87–90 of gptp_engine.cpp) ───────────────────

TEST(GptpEngineSocketFailTest, Initialize_SocketOpenFails_ReturnsFalse)
{
    auto sock = std::make_unique<FakeSocket>();
    auto identity = std::make_unique<FakeIdentity>();
    sock->SetOpenOk(false);
    GptpEngine eng{FastOptions(), std::move(sock), std::move(identity)};
    EXPECT_FALSE(eng.Initialize());
    EXPECT_TRUE(eng.Deinitialize());
}

// ── Time-jump detection (UpdateSnapshot lines 301–303) ───────────────────────

namespace
{

bool WaitForFlag(GptpEngine& eng, bool (*pred)(const score::ts::GptpIpcData&), int max_ms = 1000)
{
    for (int i = 0; i < max_ms / 10; ++i)
    {
        score::ts::GptpIpcData data{};
        eng.FinalizeSnapshot();
        eng.ReadPTPSnapshot(data);
        if (pred(data))
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

}  // namespace

TEST_F(GptpEngineFakeTest, HandlePacket_TwoSyncFup_TimeJumpFuture_Detected)
{
    ASSERT_TRUE(engine_->Initialize());

    // Pair 1: master_ns ≈ 2 s
    ::timespec hwts1{1, 0};
    socket_raw_->Push(MakeSyncFrame(1U), hwts1);
    socket_raw_->Push(MakeFollowUpFrame(1U, /*sec=*/2U, /*ns=*/0U));

    // Pair 2: master_ns ≈ 3 s (delta = 1 s > 500 ms threshold → is_time_jump_future)
    ::timespec hwts2{2, 0};
    socket_raw_->Push(MakeSyncFrame(2U), hwts2);
    socket_raw_->Push(MakeFollowUpFrame(2U, /*sec=*/3U, /*ns=*/0U));

    const bool got = WaitForFlag(*engine_, [](const score::ts::GptpIpcData& d) {
        return d.status.is_time_jump_future;
    });
    EXPECT_TRUE(got);

    engine_->FinalizeSnapshot();
    score::ts::GptpIpcData data{};
    ASSERT_TRUE(engine_->ReadPTPSnapshot(data));
    EXPECT_TRUE(data.status.is_time_jump_future);
    EXPECT_FALSE(data.status.is_correct);
}

TEST_F(GptpEngineFakeTest, HandlePacket_TwoSyncFup_TimeJumpPast_Detected)
{
    ASSERT_TRUE(engine_->Initialize());

    // Pair 1: master_ns ≈ 3 s
    ::timespec hwts1{1, 0};
    socket_raw_->Push(MakeSyncFrame(1U), hwts1);
    socket_raw_->Push(MakeFollowUpFrame(1U, /*sec=*/3U, /*ns=*/0U));

    // Pair 2: master_ns ≈ 2 s (delta < 0 → is_time_jump_past)
    ::timespec hwts2{2, 0};
    socket_raw_->Push(MakeSyncFrame(2U), hwts2);
    socket_raw_->Push(MakeFollowUpFrame(2U, /*sec=*/2U, /*ns=*/0U));

    const bool got = WaitForFlag(*engine_, [](const score::ts::GptpIpcData& d) {
        return d.status.is_time_jump_past;
    });
    EXPECT_TRUE(got);

    engine_->FinalizeSnapshot();
    score::ts::GptpIpcData data{};
    ASSERT_TRUE(engine_->ReadPTPSnapshot(data));
    EXPECT_TRUE(data.status.is_time_jump_past);
    EXPECT_FALSE(data.status.is_correct);
}

}  // namespace details
}  // namespace ts
}  // namespace score
