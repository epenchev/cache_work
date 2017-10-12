#pragma once

namespace http
{
// We assume that in most of the cases we'll receive bigger data chunks
// from the origin than from the client, thus the buffer sizes are different.
enum : bytes32_t
{
    client_rbuf_block_size = 4_KB,
    origin_rbuf_block_size = 8_KB,
};

// Constants set once at initialization time
struct constants
{
    static bytes32_t bpctrl_window_size;
};

} // namespace http
