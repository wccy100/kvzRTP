#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#if defined(__MINGW32__) || defined(__MINGW64__)
#include "mingw_inet.hh"
using namespace kvz_rtp;
using namespace mingw;
#endif

#include <cstring>
#include <iostream>

#include "debug.hh"
#include "dispatch.hh"
#include "writer.hh"

#include "formats/opus.hh"
#include "formats/hevc.hh"
#include "formats/generic.hh"

kvz_rtp::writer::writer(rtp_format_t fmt, std::string dst_addr, int dst_port):
    connection(fmt, false),
    dst_addr_(dst_addr),
    dst_port_(dst_port),
    src_port_(0)
{
}

kvz_rtp::writer::writer(rtp_format_t fmt, std::string dst_addr, int dst_port, int src_port):
    writer(fmt, dst_addr, dst_port)
{
    src_port_ = src_port;
}

kvz_rtp::writer::~writer()
{
}

rtp_error_t kvz_rtp::writer::start()
{
    rtp_error_t ret;

    if ((ret = socket_.init(AF_INET, SOCK_DGRAM, 0)) != RTP_OK)
        return ret;

    /* if source port is not 0, writer should be bind to that port so that outgoing packet
     * has a correct source port (important for hole punching purposes) */
    if (src_port_ != 0) {
        int enable = 1;

        if ((ret = socket_.setsockopt(SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(int))) != RTP_OK)
            return ret;

        LOG_DEBUG("Binding to port %d (source port)", src_port_);

        if ((ret = socket_.bind(AF_INET, INADDR_ANY, src_port_)) != RTP_OK)
            return ret;
    }

    addr_out_ = socket_.create_sockaddr(AF_INET, dst_addr_, dst_port_);
    socket_.set_sockaddr(addr_out_);

#ifdef __RTP_USE_SYSCALL_DISPATCHER__
    auto dispatcher = get_dispatcher();

    if (dispatcher)
        dispatcher->start();
#endif

    if (rtcp_ == nullptr) {
        if ((rtcp_ = new kvz_rtp::rtcp(get_ssrc(), false)) == nullptr) {
            LOG_ERROR("Failed to allocate RTCP instance!");
            return RTP_MEMORY_ERROR;
        }
    }

    return RTP_OK;
}

rtp_error_t kvz_rtp::writer::push_frame(uint8_t *data, size_t data_len, int flags)
{
    if (flags & RTP_COPY) {
        std::unique_ptr<uint8_t[]> data_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[data_len]);
        std::memcpy(data_ptr.get(), data, data_len);

        return push_frame(std::move(data_ptr), data_len, 0);
    }

    switch (get_payload()) {
        case RTP_FORMAT_HEVC:
            return kvz_rtp::hevc::push_frame(this, data, data_len, flags);

        case RTP_FORMAT_OPUS:
            return kvz_rtp::opus::push_frame(this, data, data_len, flags);

        default:
            LOG_DEBUG("Format not recognized, pushing the frame as generic");
            return kvz_rtp::generic::push_frame(this, data, data_len, flags);
    }
}

rtp_error_t kvz_rtp::writer::push_frame(std::unique_ptr<uint8_t[]> data, size_t data_len, int flags)
{
    switch (get_payload()) {
        case RTP_FORMAT_HEVC:
            return kvz_rtp::hevc::push_frame(this, std::move(data), data_len, flags);

        case RTP_FORMAT_OPUS:
            return kvz_rtp::opus::push_frame(this, std::move(data), data_len, flags);

        default:
            LOG_DEBUG("Format not recognized, pushing the frame as generic");
            return kvz_rtp::generic::push_frame(this, std::move(data), data_len, flags);
    }
}

sockaddr_in kvz_rtp::writer::get_out_address()
{
    return addr_out_;
}
