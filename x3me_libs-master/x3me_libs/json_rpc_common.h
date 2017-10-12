#pragma once

namespace x3me
{
namespace json
{
typedef rapidjson::Value value_t;
typedef rapidjson::Value::StringRefType string_ref_t;
typedef rapidjson::Document document_t;
typedef rapidjson::Document::AllocatorType value_allocator_t;

template <typename It>
struct json_stringizer
{
    using Ch = char;

    It out;
    explicit json_stringizer(It i) : out(i) {}
    void Put(char c) { *out++ = c; }
    void Flush() {}
};
template <typename It>
void json_to_string(x3me::json::value_t& json_info, It out)
{
    typedef json_stringizer<It> json_stringizer_t;
    json_stringizer_t js(out);
    rapidjson::Writer<json_stringizer_t> writer(js);
    json_info.Accept(writer);
}

namespace rpc
{

enum error_code
{
    INVALID_JSON     = -32700,
    INVALID_REQUEST  = -32600,
    METHOD_NOT_FOUND = -32601,
    INVALID_PARAMS   = -32602,
    INTERNAL_ERROR   = -32603,
};

} // namespace rpc
} // namespace json
} // namespace x3me
