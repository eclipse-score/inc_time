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
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/bpf.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define PTP_GET_TIME 0x102
#define PTP_SET_TIME 0x103
// EMAC_PTP_ADJ_FREQ_PPM: Qualcomm BSP (hw/iosock/emac_ioctl.h), ptp_ppm_t = int
// Positive ppm = speed up PHC, negative = slow down.
static constexpr unsigned long kEmacPtpAdjFreqPpm = 52UL;
struct ptp_time
{
    int64_t sec;
    int32_t nsec;
};

struct ptp_tstmp
{
    struct
    {
        std::int64_t sec;   // EMAC PHC TX hardware timestamp seconds,       offset  0
        std::int32_t nsec;  // EMAC PHC TX hardware timestamp nanoseconds,   offset  8
        // implicit 4-byte trailing pad: sizeof(this struct) = 16
    } time;
    std::uint32_t uid;  // per-TX frame matching uid (BIOCGTSTAMPID),    offset 16
    // implicit 4-byte trailing pad: sizeof(ptp_tstmp) = 24
};

#ifndef ETH_P_8021Q
#define ETH_P_8021Q 0x8100U
#endif
#ifndef ETH_P_1588
#define ETH_P_1588 0x88F7U
#endif

struct GptpEthHdr
{
    unsigned char h_dest[6];
    unsigned char h_source[6];
    uint16_t h_proto;
};

static constexpr int64_t kNsPerSec = 1'000'000'000LL;
static constexpr std::size_t kMaxBpfBufSz = 65536U;
static constexpr const char* kQnxBpfDevicePrefixEnv = "QNX_BPF_DEVICE_PREFIX";

// PHC frequency adjustment state (PI controller).
// g_skip_freq_after_step: skip N cycles after a step correction so the
//   clock can stabilise before re-applying rate-ratio based slewing.
// g_smoothed_comp_ppb: P term — EMA of raw_ppb (α=0.2), fast convergence.
// g_integral_ppb:      I term — slow integrator of P, eliminates the E/2
//   steady-state error that a pure P/EMA controller leaves behind.
static int g_skip_freq_after_step = 0;
static double g_smoothed_comp_ppb = 0.0;  // P term: EMA of raw_ppb (ppb)
static double g_integral_ppb = 0.0;       // I term: integrator (ppb)

static_assert(sizeof(ptp_tstmp) == 24U, "ptp_tstmp: time{sec:8+nsec:4+pad:4}=16 + uid:4 + pad:4 = 24");
static constexpr int kTxLoopbackCaplen = static_cast<int>(sizeof(GptpEthHdr) + sizeof(ptp_tstmp));

static struct bpf_insn kPtp1588FilterInsns[] = {
    BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETH_P_1588, 0, 1),
    BPF_STMT(BPF_RET + BPF_K, static_cast<u_int>(-1)),
    BPF_STMT(BPF_RET + BPF_K, 0),
};
static const u_int kPtp1588FilterLen = static_cast<u_int>(sizeof(kPtp1588FilterInsns) / sizeof(kPtp1588FilterInsns[0]));

static struct bpf_insn kPdelayReqFilterInsns[] = {
    BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),                 // load EtherType
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETH_P_1588, 0, 4),  // != 0x88F7 → FAIL
    BPF_STMT(BPF_LD + BPF_B + BPF_ABS, 14),                 // load PTP tsmt byte
    BPF_STMT(BPF_ALU + BPF_AND + BPF_K, 0x0FU),             // mask message type
    BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x02U, 0, 1),       // != Pdelay_Req → FAIL
    BPF_STMT(BPF_RET + BPF_K, static_cast<u_int>(-1)),      // PASS
    BPF_STMT(BPF_RET + BPF_K, 0U),                          // FAIL
};
static const u_int kPdelayReqFilterLen =
    static_cast<u_int>(sizeof(kPdelayReqFilterInsns) / sizeof(kPdelayReqFilterInsns[0]));

struct QnxRawContext
{
    int bpf_fd = -1;
    u_int bpf_buflen = 0;
    char iface_name[IFNAMSIZ]{};
    unsigned char bpf_buf[kMaxBpfBufSz]{};
    ssize_t bpf_n = 0;
    ssize_t bpf_off = 0;
    bool initialized = false;
    unsigned char tx_frame[ETHER_HDR_LEN + 1500]{};

    int promisc_sock = -1;

    int tx_lb_fd = -1;
    u_int tx_lb_buflen = 0;
    unsigned char tx_lb_buf[kMaxBpfBufSz]{};

    std::atomic<std::int64_t> inject_t1_ns{-1LL};

    ~QnxRawContext()
    {
        if (bpf_fd >= 0)
        {
            ::close(bpf_fd);
            bpf_fd = -1;
        }
        if (tx_lb_fd >= 0)
        {
            ::close(tx_lb_fd);
            tx_lb_fd = -1;
        }
        if (promisc_sock >= 0)
        {
            ::close(promisc_sock);
            promisc_sock = -1;
        }
    }
};

static QnxRawContext g_qnx_ctx;

static void bpf_ts_to_timespec(const bpf_xhdr* bh, struct timespec* ts) noexcept
{
    ts->tv_sec = static_cast<time_t>(bh->bh_tstamp.bt_sec);
    const uint64_t top32 = bh->bh_tstamp.bt_frac >> 32U;
    ts->tv_nsec = static_cast<long>((top32 * 1'000'000'000ULL) >> 32U);
}

static int ptp_payload_offset(const unsigned char* frame, int caplen)
{
    if (caplen < static_cast<int>(sizeof(GptpEthHdr)))
        return -1;
    GptpEthHdr eth{};
    std::memcpy(&eth, frame, sizeof(GptpEthHdr));
    uint16_t etype = ntohs(eth.h_proto);
    int offset = static_cast<int>(sizeof(GptpEthHdr));
    if (etype == ETH_P_8021Q)
    {
        if (caplen < offset + 4)
            return -1;
        uint16_t inner{};
        std::memcpy(&inner, frame + offset + 2, sizeof(uint16_t));
        etype = ntohs(inner);
        offset += 4;
    }
    return (etype == ETH_P_1588) ? offset : -1;
}

static void join_eth_multicast(const char* ifname, const unsigned char mac[6]) noexcept
{
    int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return;
    ::ifreq ifr{};
    ::strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    ifr.ifr_addr.sa_len = static_cast<unsigned char>(1U + 1U + ETHER_ADDR_LEN);
    ifr.ifr_addr.sa_family = AF_UNSPEC;
    std::memcpy(ifr.ifr_addr.sa_data, mac, 6);
    (void)::ioctl(s, SIOCADDMULTI, &ifr);
    ::close(s);
}

static int set_iface_promisc(const char* ifname) noexcept
{
    int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return -1;
    ::ifreq ifr{};
    ::strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (::ioctl(s, SIOCGIFFLAGS, &ifr) == 0)
    {
        ifr.ifr_flags |= IFF_PROMISC | IFF_ALLMULTI;
        (void)::ioctl(s, SIOCSIFFLAGS, &ifr);
    }
    return s;  // keep open — closed in ~QnxRawContext()
}

static int open_tx_loopback_fd(const char* ifname) noexcept
{
    char devpath[256]{};
    const char* bpf_env = std::getenv(kQnxBpfDevicePrefixEnv);
    if (bpf_env != nullptr && bpf_env[0] != '\0')
        std::snprintf(devpath, sizeof(devpath), "%s/dev/bpf0", bpf_env);
    else
        std::snprintf(devpath, sizeof(devpath), "/dev/bpf");

    const int fd = ::open(devpath, O_RDWR);
    if (fd < 0)
        return -1;

    ::ifreq ifr{};
    ::strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (::ioctl(fd, BIOCSETIF, &ifr) < 0)
    {
        ::close(fd);
        return -1;
    }

    u_int one = 1U;
    (void)::ioctl(fd, BIOCSSEESENT, &one);   // capture TX frames
    (void)::ioctl(fd, BIOCIMMEDIATE, &one);  // no batching delay

    u_int bpf_ts = BPF_T_BINTIME | BPF_T_PTP;
    (void)::ioctl(fd, BIOCSTSTAMP, &bpf_ts);

    struct bpf_program prog{kPdelayReqFilterLen, kPdelayReqFilterInsns};
    if (::ioctl(fd, BIOCSETF, &prog) < 0)
    {
        ::close(fd);
        return -1;
    }

    u_int buflen = 0U;
    if (::ioctl(fd, BIOCGBLEN, &buflen) < 0 || buflen > kMaxBpfBufSz)
    {
        ::close(fd);
        return -1;
    }
    g_qnx_ctx.tx_lb_buflen = buflen;
    return fd;
}

extern "C" int qnx_raw_open(const char* ifname)
{
    if (ifname == nullptr)
    {
        errno = EINVAL;
        return -1;
    }

    ::strlcpy(g_qnx_ctx.iface_name, ifname, sizeof(g_qnx_ctx.iface_name));

    char devpath[256]{};
    const char* bpf_env = std::getenv(kQnxBpfDevicePrefixEnv);
    if (bpf_env != nullptr && bpf_env[0] != '\0')
        std::snprintf(devpath, sizeof(devpath), "%s/dev/bpf0", bpf_env);
    else
        std::snprintf(devpath, sizeof(devpath), "/dev/bpf");

    int fd = ::open(devpath, O_RDWR);
    if (fd < 0)
        return -1;

    ::ifreq ifr{};
    ::strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (::ioctl(fd, BIOCSETIF, &ifr) < 0)
    {
        ::close(fd);
        return -1;
    }

    u_int seesent = 0U;
    (void)::ioctl(fd, BIOCSSEESENT, &seesent);

    u_int yes = 1U;
    (void)::ioctl(fd, BIOCIMMEDIATE, &yes);

    g_qnx_ctx.promisc_sock = set_iface_promisc(ifname);
    (void)::ioctl(fd, BIOCPROMISC, &yes);

    u_int bpf_ts = BPF_T_BINTIME | BPF_T_PTP;
    (void)::ioctl(fd, BIOCSTSTAMP, &bpf_ts);

    struct bpf_program prog{kPtp1588FilterLen, kPtp1588FilterInsns};
    if (::ioctl(fd, BIOCSETF, &prog) < 0)
    {
        ::close(fd);
        return -1;
    }

    if (::ioctl(fd, BIOCGBLEN, &g_qnx_ctx.bpf_buflen) < 0)
    {
        ::close(fd);
        return -1;
    }
    if (g_qnx_ctx.bpf_buflen > kMaxBpfBufSz)
    {
        ::close(fd);
        errno = ENOMEM;
        return -1;
    }

    g_qnx_ctx.bpf_fd = fd;
    g_qnx_ctx.initialized = true;

    g_qnx_ctx.tx_lb_fd = open_tx_loopback_fd(ifname);

    static const unsigned char kPtpP2PMac[6] = {0x01U, 0x80U, 0xC2U, 0x00U, 0x00U, 0x0EU};
    static const unsigned char kPtp1588Mac[6] = {0x01U, 0x1BU, 0x19U, 0x00U, 0x00U, 0x00U};
    join_eth_multicast(ifname, kPtpP2PMac);
    join_eth_multicast(ifname, kPtp1588Mac);

    return fd;
}

extern "C" int qnx_raw_recv(int fd, void* buf, int buf_len, timespec* hwts, int nonblock)
{
    if (fd < 0 || buf == nullptr || buf_len <= 0 || hwts == nullptr)
    {
        errno = EINVAL;
        return -1;
    }
    if (!g_qnx_ctx.initialized || g_qnx_ctx.bpf_buflen == 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (nonblock != 0)
    {
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags >= 0)
            (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    for (;;)
    {
        if (g_qnx_ctx.bpf_off >= g_qnx_ctx.bpf_n)
        {
            if (nonblock == 0)
            {
                struct pollfd pfd{fd, POLLIN, 0};
                const int pr = ::poll(&pfd, 1, 100);
                if (pr < 0)
                    return -1;
                if (pr == 0)
                {
                    errno = ETIMEDOUT;
                    return -1;
                }
                if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
                    return -1;
            }

            const ssize_t n = ::read(fd, g_qnx_ctx.bpf_buf, g_qnx_ctx.bpf_buflen);
            if (n < 0)
                return -1;
            if (n == 0)
            {
                if (nonblock != 0)
                {
                    errno = EAGAIN;
                    return -1;
                }
                continue;
            }
            g_qnx_ctx.bpf_n = n;
            g_qnx_ctx.bpf_off = 0;
        }

        static constexpr ssize_t kBhHdrMinBytes =
            static_cast<ssize_t>(offsetof(bpf_xhdr, bh_hdrlen)) + static_cast<ssize_t>(sizeof(u_short));  // = 26

        if (g_qnx_ctx.bpf_off + kBhHdrMinBytes > g_qnx_ctx.bpf_n)
        {
            g_qnx_ctx.bpf_off = g_qnx_ctx.bpf_n;
            continue;
        }

        const unsigned char* bh_raw = g_qnx_ctx.bpf_buf + g_qnx_ctx.bpf_off;
        bpf_u_int32 bh_caplen = 0;
        u_short bh_hdrlen = 0;
        std::memcpy(&bh_caplen, bh_raw + offsetof(bpf_xhdr, bh_caplen), sizeof(bpf_u_int32));
        std::memcpy(&bh_hdrlen, bh_raw + offsetof(bpf_xhdr, bh_hdrlen), sizeof(u_short));

        if (bh_hdrlen < static_cast<u_short>(kBhHdrMinBytes) || bh_caplen > static_cast<bpf_u_int32>(g_qnx_ctx.bpf_n) ||
            g_qnx_ctx.bpf_off + static_cast<ssize_t>(bh_hdrlen) + static_cast<ssize_t>(bh_caplen) > g_qnx_ctx.bpf_n)
        {
            g_qnx_ctx.bpf_off = g_qnx_ctx.bpf_n;
            continue;
        }

        const unsigned char* pkt = bh_raw + bh_hdrlen;
        const int caplen = static_cast<int>(bh_caplen);
        const ssize_t next_off = g_qnx_ctx.bpf_off + static_cast<ssize_t>(BPF_WORDALIGN(bh_hdrlen + bh_caplen));

        if (caplen == kTxLoopbackCaplen)
        {
            ptp_tstmp tstmp{};
            std::memcpy(&tstmp, pkt + sizeof(GptpEthHdr), sizeof(ptp_tstmp));
            const std::int64_t t1_ns = tstmp.time.sec * kNsPerSec + static_cast<std::int64_t>(tstmp.time.nsec);
            if (t1_ns > 0)
            {
                g_qnx_ctx.inject_t1_ns.store(t1_ns, std::memory_order_release);
                std::fprintf(stderr,
                             "[t1-inject] uid=%u ts=%lld.%09d\n",
                             tstmp.uid,
                             static_cast<long long>(tstmp.time.sec),
                             tstmp.time.nsec);
            }
            g_qnx_ctx.bpf_off = next_off;
            continue;
        }

        const int ptp_off = ptp_payload_offset(pkt, caplen);
        if (ptp_off < 0)
        {
            g_qnx_ctx.bpf_off = next_off;
            continue;
        }

        const uint8_t msgtype = static_cast<uint8_t>(pkt[ptp_off]) & 0x0Fu;
        const auto* bh = reinterpret_cast<const bpf_xhdr*>(bh_raw);
        bool t4_set = false;
        if (bh->bh_tstamp.bt_sec != 0 || bh->bh_tstamp.bt_frac != 0)
        {
            bpf_ts_to_timespec(bh, hwts);
            t4_set = true;
            if (msgtype == 0x03u)
            {
                std::fprintf(
                    stderr, "[t4] bpf_phc ts=%lld.%09ld\n", static_cast<long long>(hwts->tv_sec), hwts->tv_nsec);
            }
        }
        // PTP_GET_TIME fallback: use PHC hardware time when no BPF timestamp
        // is available, so rate_ratio reflects ΔPHC / ΔCLOCK_REALTIME.
        if (!t4_set && g_qnx_ctx.promisc_sock >= 0)
        {
            struct
            {
                struct ifdrv ifd;
                struct ptp_time tm;
            } cmd{};
            std::strncpy(cmd.ifd.ifd_name, g_qnx_ctx.iface_name, sizeof(cmd.ifd.ifd_name) - 1U);
            cmd.ifd.ifd_len = sizeof(cmd.tm);
            cmd.ifd.ifd_data = &cmd.tm;
            cmd.ifd.ifd_cmd = PTP_GET_TIME;
            if (::ioctl(g_qnx_ctx.promisc_sock, SIOCGDRVSPEC, &cmd) == 0)
            {
                hwts->tv_sec = static_cast<time_t>(cmd.tm.sec);
                hwts->tv_nsec = static_cast<long>(cmd.tm.nsec);
                t4_set = true;
                if (msgtype == 0x03u)
                {
                    std::fprintf(stderr,
                                 "[t4] PTP_GET_TIME ts=%lld.%09ld\n",
                                 static_cast<long long>(cmd.tm.sec),
                                 static_cast<long>(cmd.tm.nsec));
                }
            }
        }
        if (!t4_set)
        {
            (void)::clock_gettime(CLOCK_REALTIME, hwts);
        }

        const int frame_len = std::min(caplen, buf_len);
        std::memcpy(buf, pkt, static_cast<std::size_t>(frame_len));
        g_qnx_ctx.bpf_off = next_off;
        return frame_len;
    }
}

extern "C" int qnx_raw_send(int fd, const void* buf, int len, timespec* hwts)
{
    if (fd < 0 || buf == nullptr || len <= 0 || hwts == nullptr)
    {
        errno = EINVAL;
        return -1;
    }
    if (static_cast<unsigned int>(len) > 1500U)
    {
        errno = EMSGSIZE;
        return -1;
    }

    std::memcpy(g_qnx_ctx.tx_frame, buf, static_cast<std::size_t>(len));

    g_qnx_ctx.inject_t1_ns.store(-1LL, std::memory_order_relaxed);

    if (::write(fd, g_qnx_ctx.tx_frame, static_cast<std::size_t>(len)) < 0)
        return -1;

    for (int i = 0; i < 100; ++i)
    {
        const std::int64_t t1 = g_qnx_ctx.inject_t1_ns.load(std::memory_order_acquire);
        if (t1 > 0)
        {
            hwts->tv_sec = static_cast<time_t>(t1 / kNsPerSec);
            hwts->tv_nsec = static_cast<long>(t1 % kNsPerSec);
            std::fprintf(stderr, "[t1] inject   ts=%lld.%09ld\n", static_cast<long long>(hwts->tv_sec), hwts->tv_nsec);
            return len;
        }
        ::usleep(100U);  // 100 µs
    }

    if (g_qnx_ctx.promisc_sock >= 0)
    {
        struct
        {
            struct ifdrv ifd;
            struct ptp_time tm;
        } cmd{};
        std::strncpy(cmd.ifd.ifd_name, g_qnx_ctx.iface_name, sizeof(cmd.ifd.ifd_name) - 1U);
        cmd.ifd.ifd_len = sizeof(cmd.tm);
        cmd.ifd.ifd_data = &cmd.tm;
        cmd.ifd.ifd_cmd = PTP_GET_TIME;
        if (::ioctl(g_qnx_ctx.promisc_sock, SIOCGDRVSPEC, &cmd) == 0)
        {
            hwts->tv_sec = static_cast<time_t>(cmd.tm.sec);
            hwts->tv_nsec = static_cast<long>(cmd.tm.nsec);
            std::fprintf(stderr,
                         "[t1] PTP_GET  ts=%lld.%09ld (inject timeout)\n",
                         static_cast<long long>(hwts->tv_sec),
                         hwts->tv_nsec);
            return len;
        }
    }

    (void)::clock_gettime(CLOCK_REALTIME, hwts);
    std::fprintf(stderr,
                 "[t1] CLOCK_RT ts=%lld.%09ld (fallback)\n",
                 static_cast<long long>(hwts->tv_sec),
                 static_cast<long>(hwts->tv_nsec));
    return len;
}

extern "C" int qnx_phc_open(const char* phc_dev)
{
    if (phc_dev != nullptr && phc_dev[0] != '\0' && phc_dev[0] != '/')
        ::strlcpy(g_qnx_ctx.iface_name, phc_dev, sizeof(g_qnx_ctx.iface_name));
    return 0;
}

extern "C" int qnx_phc_adjtime_step(int /*phc_fd*/, long long offset_ns)
{
    if (offset_ns == 0)
        return 0;

    const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return -1;

    struct
    {
        struct ifdrv ifd;
        struct ptp_time tm;
    } cmd{};
    std::strncpy(cmd.ifd.ifd_name, g_qnx_ctx.iface_name, sizeof(cmd.ifd.ifd_name) - 1U);
    cmd.ifd.ifd_len = sizeof(cmd.tm);
    cmd.ifd.ifd_data = &cmd.tm;
    cmd.ifd.ifd_cmd = PTP_GET_TIME;

    if (::ioctl(s, SIOCGDRVSPEC, &cmd) == -1)
    {
        ::close(s);
        return -1;
    }

    const int64_t cur_ns = cmd.tm.sec * kNsPerSec + static_cast<int64_t>(cmd.tm.nsec);
    const int64_t new_ns = cur_ns - static_cast<int64_t>(offset_ns);
    cmd.tm.sec = new_ns / kNsPerSec;
    cmd.tm.nsec = static_cast<int32_t>(new_ns % kNsPerSec);
    if (cmd.tm.nsec < 0)
    {
        cmd.tm.nsec += static_cast<int32_t>(kNsPerSec);
        cmd.tm.sec -= 1;
    }

    cmd.ifd.ifd_cmd = PTP_SET_TIME;
    const int r = ::ioctl(s, SIOCGDRVSPEC, &cmd);
    if (r == 0)
    {
        std::fprintf(stderr,
                     "[phc-step] offset=%lld ns  new=%lld.%09d\n",
                     static_cast<long long>(offset_ns),
                     static_cast<long long>(cmd.tm.sec),
                     cmd.tm.nsec);
        // After a hard step, skip 3 frequency-adjustment cycles and reset
        // the smoothed estimate so stale rate data doesn't corrupt slewing.
        g_skip_freq_after_step = 3;
        g_smoothed_comp_ppb = 0.0;
        g_integral_ppb = 0.0;
    }
    else
    {
        std::fprintf(stderr, "[phc-step] PTP_SET_TIME failed errno=%d\n", errno);
    }
    ::close(s);
    return r;
}

extern "C" int qnx_phc_adjfreq_ppb(int /*phc_fd*/, long long freq_ppb)
{
    // Skip a few cycles immediately after a step correction.
    if (g_skip_freq_after_step > 0)
    {
        --g_skip_freq_after_step;
        return 0;
    }

    constexpr double kAlpha = 0.2;
    constexpr double kKi = 0.002;
    constexpr double kICap = 300'000.0;    // I term anti-windup: ±300 ppm
    constexpr double kTotCap = 400'000.0;  // combined output cap: ±400 ppm

    // --- P term: EMA of raw_ppb ---
    g_smoothed_comp_ppb = kAlpha * static_cast<double>(freq_ppb) + (1.0 - kAlpha) * g_smoothed_comp_ppb;

    // --- I term: slow integrator of P; clamp to prevent wind-up ---
    g_integral_ppb += kKi * g_smoothed_comp_ppb;
    if (g_integral_ppb > kICap)
        g_integral_ppb = kICap;
    if (g_integral_ppb < -kICap)
        g_integral_ppb = -kICap;

    // --- Combined PI output ---
    double combined = g_smoothed_comp_ppb + g_integral_ppb;
    if (combined > kTotCap)
        combined = kTotCap;
    if (combined < -kTotCap)
        combined = -kTotCap;

    // ppb → ppm with sign flip:
    //   positive error = slave running fast → apply negative adj_ppm to slow PHC down
    const int adj_ppm = -static_cast<int>(combined / 1000.0);
    if (adj_ppm == 0)
        return 0;  // below 1 ppm resolution, skip ioctl

    const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return -1;

    struct
    {
        struct ifdrv ifd;
        int ppm;
    } cmd{};
    std::strncpy(cmd.ifd.ifd_name, g_qnx_ctx.iface_name, sizeof(cmd.ifd.ifd_name) - 1U);
    cmd.ifd.ifd_cmd = kEmacPtpAdjFreqPpm;
    cmd.ifd.ifd_len = sizeof(int);
    cmd.ifd.ifd_data = &cmd.ppm;
    cmd.ppm = adj_ppm;

    const int r = ::ioctl(s, SIOCGDRVSPEC, &cmd);
    std::fprintf(stderr,
                 "[phc-freq] raw_ppb=%lld P=%.0f I=%.0f adj_ppm=%d r=%d%s\n",
                 static_cast<long long>(freq_ppb),
                 g_smoothed_comp_ppb,
                 g_integral_ppb,
                 adj_ppm,
                 r,
                 r != 0 ? " (FAILED)" : "");
    ::close(s);
    return r;
}
