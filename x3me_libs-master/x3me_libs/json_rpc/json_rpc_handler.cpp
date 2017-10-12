#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>

#include "../http/http_resp.h"

#include "json_rpc_handler.h"

namespace x3me
{
namespace net
{
namespace json_rpc
{
namespace detail
{

class buff_writer
{
    std::vector<uint8_t>& buff_;

public:
    typedef char Ch; // The RapidJSON library needs this typedef

public:
    buff_writer(std::vector<uint8_t>& buff, size_t init_capacity) : buff_(buff)
    {
        buff_.reserve(init_capacity);
    }
    ~buff_writer() = default;

    buff_writer() = delete;
    buff_writer(const buff_writer&) = delete;
    buff_writer& operator=(const buff_writer&) = delete;
    buff_writer(buff_writer&&) = delete;
    buff_writer& operator=(buff_writer&&) = delete;

    // Only these two methods are needed for writers.
    // Only the Put method is needed for our writer
    void Put(Ch c) { buff_.push_back(static_cast<uint8_t>(c)); }
    void Flush() {}
};

////////////////////////////////////////////////////////////////////////////////

void write_http_response(const http_conn_ptr_t& conn,
                         const document_t& res) noexcept
{
    if (!res.IsNull())
    {
        http_resp resp;

        buff_writer bwriter(resp.body(), 1024);
        rapidjson::PrettyWriter<buff_writer> writer(bwriter);
        res.Accept(writer);

        auto& hdrs = resp.headers();
        hdrs.erase("content-type"); // The library works with small caps only
        hdrs.emplace("content-type", "application/json");

        conn->async_write_resp(http_status_code::ok, resp);
    }
}

void write_response(document_t&& res, uint64_t in_msg_id,
                    const http_conn_ptr_t& conn) noexcept
{
    document_t out_json;
    auto& alloc = out_json.GetAllocator();

    value_t null(rapidjson::kNullType);

    out_json.SetObject();
    if (in_msg_id != json_rpc_res::invalid_id)
    {
        out_json.AddMember("id", in_msg_id, alloc);
    }
    out_json.AddMember("jsonrpc", "2.0", alloc);
    out_json.AddMember("result", res, alloc);
    // This is wrong by the JSON-RPC 2.0 specification, but
    // I added it in order to support, kind-of, version 1.0.
    out_json.AddMember("error", null, alloc);

    write_http_response(conn, out_json);
}

void write_error_response(error_code err, string_ref_t err_msg,
                          uint64_t in_msg_id,
                          const http_conn_ptr_t& conn) noexcept
{
    document_t out_json;
    auto& alloc = out_json.GetAllocator();

    value_t null(rapidjson::kNullType);

    value_t e_msg(err_msg.s, err_msg.length, alloc);
    value_t err_obj(rapidjson::kObjectType);
    err_obj.AddMember("code", static_cast<int32_t>(err), alloc);
    err_obj.AddMember("message", e_msg, alloc);

    out_json.SetObject();

    if (in_msg_id != json_rpc_res::invalid_id)
    {
        if ((err == error_code::procedure_not_found) ||
            (err == error_code::internal_error))
        {
            out_json.AddMember("id", in_msg_id, alloc);
        }
        else
        {
            out_json.AddMember("id", null, alloc);
        }
    }

    out_json.AddMember("jsonrpc", "2.0", alloc);
    out_json.AddMember("result", null, alloc);
    out_json.AddMember("error", err_obj, alloc);

    write_http_response(conn, out_json);
}

////////////////////////////////////////////////////////////////////////////////

void verify_in_json(value_t& in_json)
{
    if (!in_json.IsObject())
    {
        throw error(error_code::invalid_request,
                    "Root element needs to be object");
    }
    if (!in_json.HasMember("method") || !in_json["method"].IsString())
    {
        throw error(error_code::invalid_request,
                    "Invalid JSON-RPC request. Field "
                    "'method' is wrong type, not string, or missing");
    }
    if (!in_json.HasMember("params") || !in_json["params"].IsArray())
    {
        throw error(error_code::invalid_request,
                    "Invalid JSON-RPC request. Field "
                    "'params' is wrong type, not array, or missing. Named "
                    "parameters are not supported.");
    }
}

uint64_t get_msg_id(const value_t& in_json) noexcept
{
    if (in_json.IsObject())
    {
        auto it = in_json.FindMember("id");
        if (it != in_json.MemberEnd())
        {
            if (it->value.IsUint())
                return it->value.GetUint();
            else if (it->value.IsUint())
                return it->value.GetUint64();
        }
    }
    return json_rpc_res::invalid_id;
}

////////////////////////////////////////////////////////////////////////////////

handler::handler() noexcept
{
}

handler::~handler() noexcept
{
}

handler::handler(handler&& rhs) noexcept : callbacks_(std::move(rhs.callbacks_))
{
}

handler& handler::operator=(handler&& rhs) noexcept
{
    callbacks_ = std::move(rhs.callbacks_);
    return *this;
}

void handler::process(std::vector<uint8_t>& in,
                      const http_conn_ptr_t& conn) noexcept
{
    uint64_t in_msg_id = json_rpc_res::invalid_id;

    try
    {
        document_t in_json;

        // This is sad but the rapidjson wants non const pointer even if
        // it doesn't change it.
        // The rapidjson library needs null terminated string.
        in.push_back(0);
        auto p = reinterpret_cast<char*>(&in[0]);
        if (in_json.ParseInsitu(p).HasParseError())
        {
            throw error(error_code::invalid_json,
                        rapidjson::GetParseError_En(in_json.GetParseError()));
        }

        verify_in_json(in_json);

        in_msg_id = get_msg_id(in_json);

        const auto method_name = in_json["method"].GetString();
        auto found = callbacks_.find(method_name);
        if (found == callbacks_.end())
        {
            throw error(error_code::procedure_not_found,
                        std::string("Procedure '") + method_name +
                            "' not found");
        }

        json_rpc_res res(conn, in_msg_id);
        found->second->exec(res, in_json["params"]);
    }
    catch (const error& err)
    {
        write_error_response(err.code(), rapidjson::StringRef(err.msg().data(),
                                                              err.msg().size()),
                             in_msg_id, conn);
    }
}

} // namespace detail
////////////////////////////////////////////////////////////////////////////////

void json_rpc_res::write_response(document_t&& doc) noexcept
{
    detail::write_response(std::move(doc), res_id_, conn_);
}

void json_rpc_res::write_error_response(string_ref_t err_msg) noexcept
{
    detail::write_error_response(error_code::internal_error, err_msg, res_id_,
                                 conn_);
}

} // namespace json_rpc
} // namespace net
} // namesapce x3me
