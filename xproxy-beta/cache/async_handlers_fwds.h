#pragma once

namespace cache
{
namespace detail
{
class object_open_handle;
class object_read_handle;
class object_write_handle;
using object_ohandle_ptr_t = boost::intrusive_ptr<object_open_handle>;
using object_rhandle_ptr_t = boost::intrusive_ptr<object_read_handle>;
using object_whandle_ptr_t = boost::intrusive_ptr<object_write_handle>;

using open_rhandler_t =
    std::function<void(const err_code_t&, detail::object_rhandle_ptr_t&&)>;
using open_whandler_t =
    std::function<void(const err_code_t&, detail::object_whandle_ptr_t&&)>;

} // namespace detail
} // namespace cache
