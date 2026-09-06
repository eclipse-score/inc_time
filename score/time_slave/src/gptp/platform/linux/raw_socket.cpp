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
#include "score/time_slave/src/gptp/details/raw_socket_impl.h"

#include "score/mw/log/logging.h"
#include "score/time_slave/src/common/logging_contexts.h"

#include <arpa/inet.h>
#include <linux/filter.h>
#include <linux/if_ether.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace score
{
namespace ts
{
namespace details
{

namespace
{

void DrainErrQueue(int fd, OsSyscalls& sys) noexcept
{
    char buf[2048];
    ::iovec iov{buf, sizeof(buf)};
    char ctrl[2048];
    ::msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl;
    msg.msg_controllen = sizeof(ctrl);

    while (sys.recvmsg_call(fd, &msg, MSG_ERRQUEUE) > 0)
    {
    }
}

}  // namespace

RawSocketImpl::RawSocketImpl(OsSyscalls* sys) noexcept : sys_{sys != nullptr ? sys : &RealOsSyscalls::Instance()} {}

RawSocketImpl::~RawSocketImpl()
{
    Close();
}

bool RawSocketImpl::Open(const std::string& iface)
{
    Close();

    const int fd = sys_->socket_call(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0)
    {
        score::mw::log::LogError(kTimeSlaveAppContext)
            << "RawSocket::Open: Failed to create raw socket endpoint (errno=" << errno << ")";
        return false;
    }

    ::ifreq ifr{};
    std::strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    if (sys_->ioctl_call(fd, SIOCGIFINDEX, &ifr) < 0)
    {
        sys_->close_call(fd);
        score::mw::log::LogError(kTimeSlaveAppContext)
            << "RawSocket::Open: Failed to manipulate raw socket endpoint (errno=" << errno << ")";
        return false;
    }

    ::sockaddr_ll sa{};
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(ETH_P_ALL);
    sa.sll_ifindex = ifr.ifr_ifindex;
    if (sys_->bind_call(fd, reinterpret_cast<::sockaddr*>(&sa), sizeof(sa)) < 0)
    {
        sys_->close_call(fd);
        score::mw::log::LogError(kTimeSlaveAppContext)
            << "RawSocket::Open: Failed to bind raw socket endpoint to interface " << iface << " (errno=" << errno
            << ")";
        return false;
    }

    // SO_BINDTODEVICE: best-effort, don't fail if it doesn't work
    (void)sys_->setsockopt_call(fd, SOL_SOCKET, SO_BINDTODEVICE, iface.c_str(), static_cast<socklen_t>(iface.size()));

    // Enable promiscuous mode so the NIC passes all frames (including PTP multicast) to the kernel.
    ::packet_mreq mr{};
    mr.mr_ifindex = ifr.ifr_ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    if (sys_->setsockopt_call(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) < 0)
    {
        score::mw::log::LogWarn(kTimeSlaveAppContext)
            << "RawSocket::Open: Failed to set promiscuous mode (errno=" << errno << ")";
    }

    // BPF filter: pass only frames with EtherType 0x88F7 (PTP/gPTP). This is fine ss gPTP frames
    // are n9ot VLAN (nor priority) tagged. The BPF filter runs in-kernel before delivery
    // to userspace, so non-PTP frames are dropped with zero overhead in the application.
    static const ::sock_filter kPtpBpfCode[] = {
        BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 12),                    // load EtherType
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ETH_P_1588, 1, 0),     // == 0x88F7 → PASS
        BPF_STMT(BPF_RET | BPF_K, 0U),                             // FAIL: drop
        BPF_STMT(BPF_RET | BPF_K, static_cast<unsigned int>(-1)),  // PASS: accept
    };
    ::sock_fprog prog{};
    prog.len = static_cast<unsigned short>(sizeof(kPtpBpfCode) / sizeof(kPtpBpfCode[0]));
    prog.filter = const_cast<::sock_filter*>(kPtpBpfCode);
    if (sys_->setsockopt_call(fd, SOL_SOCKET, SO_ATTACH_FILTER, &prog, sizeof(prog)) < 0)
    {
        score::mw::log::LogWarn(kTimeSlaveAppContext)
            << "RawSocket::Open: Failed to set BPF filter (errno=" << errno << ")";
    }

    // Enable kernel layer software timestamps as a baseline, so we get at least some timestamp
    // even if hw timestamping isn't available or fails to configure. The HW timestamp will come
    // in the same control message but different field, so the caller can use it if present and
    // fall back to SW timestamp if not.
    const int enable = 1;
    if (sys_->setsockopt_call(fd, SOL_SOCKET, SO_TIMESTAMPNS, &enable, sizeof(enable)) < 0)
    {
        score::mw::log::LogWarn(kTimeSlaveAppContext)
            << "RawSocket::Open: Failed to enable SO_TIMESTAMPNS (errno=" << errno << ")";
    }

    fd_.store(fd, std::memory_order_release);
    iface_ = iface;
    return true;
}

bool RawSocketImpl::EnableHwTimestamping()
{
    const int fd = fd_.load(std::memory_order_relaxed);
    if (fd < 0)
        return false;

    ::ifreq ifr{};
    ::hwtstamp_config cfg{};
    std::strncpy(ifr.ifr_name, iface_.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_data = reinterpret_cast<char*>(&cfg);

    cfg.tx_type = HWTSTAMP_TX_ON;
    cfg.rx_filter = HWTSTAMP_FILTER_ALL;

    if (sys_->ioctl_call(fd, SIOCSHWTSTAMP, &ifr) < 0)
    {
        // Fall back to PTP-only filter
        cfg.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
        (void)sys_->ioctl_call(fd, SIOCSHWTSTAMP, &ifr);
    }

    const int ts_opts = SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE;
    if (sys_->setsockopt_call(fd, SOL_SOCKET, SO_TIMESTAMPING, &ts_opts, sizeof(ts_opts)) < 0)
    {
        return false;
    }
    return true;
}

void RawSocketImpl::Close()
{
    const int fd = fd_.exchange(-1, std::memory_order_acq_rel);
    if (fd >= 0)
        sys_->close_call(fd);
    iface_.clear();
}

int RawSocketImpl::Recv(std::uint8_t* buf, std::size_t buf_len, ::timespec& hwts, int timeout_ms)
{
    const int fd = fd_.load(std::memory_order_acquire);
    if (fd < 0 || buf == nullptr || buf_len == 0)
        return -1;

    // Poll with caller-specified timeout
    ::pollfd pfd{fd, POLLIN, 0};
    const int pr = sys_->poll_call(&pfd, 1, timeout_ms);
    if (pr == 0)
        return 0;  // timeout
    if (pr < 0)
        return -1;

    char ctrl[1024];
    ::iovec iov{buf, buf_len};
    ::msghdr msg{};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrl;
    msg.msg_controllen = sizeof(ctrl);

    const int len = static_cast<int>(sys_->recvmsg_call(fd, &msg, 0));
    if (len < 0)
        return -1;

    std::memset(&hwts, 0, sizeof(hwts));
    for (::cmsghdr* cm = CMSG_FIRSTHDR(&msg); cm != nullptr; cm = CMSG_NXTHDR(&msg, cm))
    {
        if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SO_TIMESTAMPING)
        {
            if (cm->cmsg_len < CMSG_LEN(3 * sizeof(::timespec)))
                continue;
            const auto* ts = reinterpret_cast<const ::timespec*>(CMSG_DATA(cm));
            if (ts[2].tv_sec != 0 || ts[2].tv_nsec != 0)
            {
                hwts = ts[2];
                break;  // Prefer raw HW timestamp if available
            }
        }
        else if (cm->cmsg_type == SO_TIMESTAMPNS)
        {
            memcpy(&hwts, CMSG_DATA(cm), sizeof(hwts));
        }
    }
    return len;
}

int RawSocketImpl::Send(const void* buf, int len, ::timespec& hwts)
{
    const int fd = fd_.load(std::memory_order_acquire);
    if (fd < 0 || buf == nullptr || len <= 0)
        return -1;

    DrainErrQueue(fd, *sys_);

    const int sent = static_cast<int>(sys_->send_call(fd, buf, static_cast<std::size_t>(len), 0));
    if (sent < 0)
        return -1;

    constexpr int kTxTsTimeoutMs = 50;
    ::pollfd pfd{fd, POLLERR, 0};
    std::memset(&hwts, 0, sizeof(hwts));
    if (sys_->poll_call(&pfd, 1, kTxTsTimeoutMs) > 0 && (pfd.revents & POLLERR) != 0)
    {
        std::uint8_t tmp[2048];
        ::iovec iov{tmp, sizeof(tmp)};
        char ctrl[512];
        ::msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ctrl;
        msg.msg_controllen = sizeof(ctrl);

        if (sys_->recvmsg_call(fd, &msg, MSG_ERRQUEUE) >= 0)
        {
            for (::cmsghdr* cm = CMSG_FIRSTHDR(&msg); cm != nullptr; cm = CMSG_NXTHDR(&msg, cm))
            {
                if (cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SO_TIMESTAMPING)
                {
                    // SO_TIMESTAMPING delivers three timespec values: [0]=SW, [1]=HW-transformed,
                    // [2]=HW-raw.  Verify the cmsg payload is large enough before indexing ts[2].
                    if (cm->cmsg_len < CMSG_LEN(3 * sizeof(::timespec)))
                        continue;
                    const auto* ts = reinterpret_cast<const ::timespec*>(CMSG_DATA(cm));
                    if (ts[2].tv_sec != 0 || ts[2].tv_nsec != 0)
                        hwts = ts[2];
                }
            }
        }
    }
    return sent;
}

}  // namespace details
}  // namespace ts
}  // namespace score
