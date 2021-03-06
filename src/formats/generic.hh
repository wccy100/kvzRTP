#pragma once

#include <vector>

#include "frame.hh"
#include "util.hh"

namespace kvz_rtp {
    class connection;
    class reader;

    namespace generic {

        /* TODO:  */
        rtp_error_t push_frame(kvz_rtp::connection *conn, uint8_t *data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t push_frame(kvz_rtp::connection *conn, std::unique_ptr<uint8_t[]> data, size_t data_len, int flags);

        /* TODO:  */
        rtp_error_t frame_receiver(kvz_rtp::reader *reader);
    };
};
