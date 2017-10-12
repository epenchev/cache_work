#include "precompiled.h"
#include "handler_factory.h"
#include "http_bp_ctl.h"
#include "http_handler.h"

namespace http
{

net::handler_factory_t make_handler_factory(cache::object_distributor& cod,
                                            all_stats& stats,
                                            http_bp_ctl& bp_ctl) noexcept
{
    return [&cod, &stats,
            &bp_ctl](const id_tag& tag, io_service_t& ios,
                     net_thread_id_t net_tid) -> net::proto_handler_ptr_t
    {
        return std::make_unique<detail::http_handler>(tag, cod, ios, net_tid,
                                                      stats, bp_ctl);
    };
}

} // namespace http
