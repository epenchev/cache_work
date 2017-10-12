#pragma once

#include "net/public_types.h"

namespace cache
{
class object_distributor;
} // namespace cache
namespace http
{
class all_stats;
class http_bp_ctl;

net::handler_factory_t make_handler_factory(cache::object_distributor& cod,
                                            all_stats& stats,
                                            http_bp_ctl& bp_ctl) noexcept;

} // namespace http
