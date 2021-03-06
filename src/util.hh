#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>


#if defined(__MINGW32__) || defined(__MINGW64__) || defined(__linux__)
#define PACKED_STRUCT(name) \
    struct __attribute__((packed)) name
#else
#warning "structures are not packed!"
#define PACKED_STRUCT(name) struct name
#endif

const int MAX_PACKET      = 65536;
const int MAX_PAYLOAD     = 1441;

typedef enum RTP_ERROR {
    RTP_INTERRUPTED    =  2,
    RTP_NOT_READY      =  1,
    RTP_OK             =  0,
    RTP_GENERIC_ERROR  = -1,
    RTP_SOCKET_ERROR   = -2,
    RTP_BIND_ERROR     = -3,
    RTP_INVALID_VALUE  = -4,
    RTP_SEND_ERROR     = -5,
    RTP_MEMORY_ERROR   = -6,
    RTP_SSRC_COLLISION = -7,
} rtp_error_t;

typedef enum RTP_FORMAT {
    RTP_FORMAT_GENERIC = 0,
    RTP_FORMAT_HEVC    = 96,
    RTP_FORMAT_OPUS    = 97,
} rtp_format_t;

typedef enum RTP_FLAGS {
    RTP_NO_FLAGS = 0 << 0,

    /* TODO  */
    RTP_SLICE    = 1 << 0,

    /* TODO */
    RTP_MORE     = 1 << 1,

    /* Make a copy of "data" given to push_frame()
     *
     * This is an easy way out of the memory ownership/deallocation problem
     * for the application but can significantly damage the performance
     *
     * NOTE: Copying is necessary only when the following conditions are met:
     *   - SCD is enabled
     *   - Media format is such that it uses SCD (HEVC is the only for now)
     *   - Application hasn't provided a deallocation callback
     *   - Application doens't use unique_ptrs
     *
     * If all of those conditions are met, then it's advised to pass RTP_COPY.
     * Otherwise there might be a lot of leaked memory */
    RTP_COPY = 1 << 2,
} rtp_flags_t;

extern thread_local rtp_error_t rtp_errno;

static inline void hex_dump(uint8_t *buf, size_t len)
{
    if (!buf)
        return;

    for (size_t i = 0; i < len; i += 10) {
        fprintf(stderr, "\t");
        for (size_t k = i; k < i + 10; ++k) {
            fprintf(stderr, "0x%02x ", buf[k]);
        }
        fprintf(stderr, "\n");
    }
}

static inline void set_bytes(int *ptr, int nbytes)
{
    if (ptr)
        *ptr = nbytes;
}

static inline std::string generate_string(size_t length)
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };

    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}
