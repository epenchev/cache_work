#include "precompiled.h"
#include "json_rpc.h"

namespace json_rpc
{

std::string json_to_string(const rapidjson::Value& val) noexcept
{
    class json_stringizer
    {
        std::string& out_;

    public:
        explicit json_stringizer(std::string& s) noexcept : out_(s) {}
        void Put(char c) noexcept { out_.push_back(c); }
        void Flush() noexcept {}
    };

    std::string ret;
    ret.reserve(512);
    json_stringizer js(ret);
    rapidjson::PrettyWriter<json_stringizer> writer(js);
    val.Accept(writer);

    return ret;
}

std::string get_json_rpc_resp(std::string& resp) noexcept
{
    resp += '\0'; // Just to be sure because we aren't going to use the c_str().

    rapidjson::Document in_json;

    if (in_json.ParseInsitu(&resp[0]).HasParseError())
    {
        return rapidjson::GetParseError_En(in_json.GetParseError());
    }

    {
        auto it = in_json.FindMember("error");
        if ((it != in_json.MemberEnd()) && it->value.IsObject())
        {
            auto it2 = it->value.FindMember("message");
            if ((it2 != it->value.MemberEnd()) && it2->value.IsString())
            {
                const auto& s = it2->value;
                return std::string(s.GetString(), s.GetStringLength());
            }
        }
    }

    {
        auto it = in_json.FindMember("result");
        if (it != in_json.MemberEnd())
        {
            return json_to_string(it->value);
        }
    }

    return "Invalid JSON-RPC response. " + resp;
}

} // namespace json_rpc
