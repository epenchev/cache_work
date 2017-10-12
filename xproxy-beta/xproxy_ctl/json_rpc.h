#pragma once

// There is no currently available JSON-PRC client functionality.
// So, this one, created in hurry, is used instead of it. For now ...
namespace json_rpc
{

using value_t      = rapidjson::Value;
using string_ref_t = rapidjson::Value::StringRefType;
using document_t   = rapidjson::Document;
using allocator_t  = rapidjson::Document::AllocatorType;

namespace detail
{

// Support strings and numbers
template <typename T>
void push_back_impl(value_t& arr, allocator_t& alloc, const T& v,
                    std::false_type /*is_number*/) noexcept
{
    using namespace x3me;
    arr.PushBack(string_ref_t(utils::data(v), utils::size(v)), alloc);
}

template <typename T>
void push_back_impl(value_t& arr, allocator_t& alloc, const T& v,
                    std::true_type /*is_number*/) noexcept
{
    arr.PushBack(v, alloc);
}

template <typename T>
void push_back(value_t& arr, allocator_t& alloc, const T& v) noexcept
{
    using is_number_t =
        std::integral_constant<bool, std::is_integral<T>::value ||
                                         std::is_floating_point<T>::value>;
    push_back_impl(arr, alloc, v, is_number_t{});
}

} // namespace detail
////////////////////////////////////////////////////////////////////////////////

std::string json_to_string(const value_t& val) noexcept;

template <typename... Args>
std::string prepare_json_rpc_req(string_view_t method,
                                 const Args&... ps) noexcept
{
    document_t all;
    auto& alloc = all.GetAllocator();

    value_t params(rapidjson::kArrayType);
    int _[] = {(detail::push_back(params, alloc, ps), 0)...};
    (void)_;

    all.SetObject();
    all.AddMember("method", string_ref_t(method.data(), method.size()), alloc);
    all.AddMember("params", params, alloc);
    all.AddMember("id", 42, alloc);

    return json_to_string(all);
}

std::string get_json_rpc_resp(std::string& resp) noexcept;

} // namespace json_rpc
