#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include <boost/asio/ip/address_v4.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/system/error_code.hpp>
#include <boost/utility/string_view.hpp>

#include <fmt/ostream.h>

#include <x3me_libs/math_funcs.h>
#include <x3me_libs/shared_mutex.h>
#include <x3me_libs/stack_string.h>
#include <x3me_libs/sys_utils.h>
#include <x3me_libs/utils.h>

using err_code_t    = boost::system::error_code;
using string_view_t = boost::string_view;
template <size_t Size>
using stack_string_t = x3me::str_utils::stack_string<Size>;

#include "../../xlog/logger.h"
#include "../../xlog/logger.ipp"
#include "../../xlog/async_channel.h"
#include "../../xlog/log_target.h"
