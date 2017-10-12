#pragma once

#include <rapidjson/document.h>

#include "../http/http_conn.h"

namespace x3me
{
namespace net
{

using http_conn_ptr_t = std::shared_ptr<http_conn>;

namespace json_rpc
{
namespace detail
{
class handler;
} // namespace detail
////////////////////////////////////////////////////////////////////////////////

using value_t      = rapidjson::Value;
using string_ref_t = rapidjson::Value::StringRefType;
using document_t   = rapidjson::Document;
using allocator_t  = rapidjson::Document::AllocatorType;

class json_rpc_res
{
public:
    static constexpr uint64_t invalid_id = ~uint64_t(0);

private:
    http_conn_ptr_t conn_;
    const uint64_t res_id_;

    friend class json_rpc::detail::handler;
    explicit json_rpc_res(const http_conn_ptr_t& c, uint64_t res_id) noexcept
        : conn_(c),
          res_id_(res_id)
    {
    }

public:
    ~json_rpc_res() noexcept = default;

    void write_response(document_t&& doc) noexcept;
    void write_error_response(string_ref_t err_msg) noexcept;
};

enum struct error_code
{
    invalid_json        = -32700,
    invalid_request     = -32600,
    procedure_not_found = -32601,
    invalid_params      = -32602,
    internal_error      = -32603,
};

class error
{
    std::string msg_;
    error_code code_;

public:
    error(error_code c, const char* m) : msg_(m), code_(c) {}
    error(error_code c, const std::string& m) : msg_(m), code_(c) {}

    const auto& msg() const noexcept { return msg_; }
    const auto& code() const noexcept { return code_; }
};

} // namespace json_rpc
} // namespace net
} // namespace x3me
