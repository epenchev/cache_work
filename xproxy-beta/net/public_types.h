#pragma once

namespace net
{
class proto_handler;
using proto_handler_ptr_t = std::unique_ptr<proto_handler>;
using handler_factory_t = std::function<proto_handler_ptr_t(
    const id_tag&, io_service_t&, net_thread_id_t)>;

// The Linux kernel also uses stack allocation for up to 8 entries, for the
// scatter/gather IO trying to avoid heap allocations.
template <typename Buff>
using vec_buffer_t    = boost::container::small_vector<Buff, 8>;
using vec_wr_buffer_t = vec_buffer_t<boost::asio::mutable_buffer>;
using vec_ro_buffer_t = vec_buffer_t<boost::asio::const_buffer>;

using handler_t = x3me::utils::inplace_fn<x3me::utils::inplace_params<8>,
                                          void(const err_code_t&, bytes32_t)>;

} // namespace net
