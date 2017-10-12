#pragma once

namespace plgns
{

// Executes the given function in the net thread given by the net_thread_id
using net_thread_executor_t =
    std::function<void(net_thread_id_t, std::function<void()>)>;

struct net_thread_exec
{
    net_thread_id_t cnt_threads_;
    net_thread_executor_t exec_;
};

} // namespace plgns
