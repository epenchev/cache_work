#include "types.h"
#include "json_rpc_handler.h"
#include "string_builder.h"

namespace x3me
{
namespace json
{
namespace rpc
{


void prepare_error_response(value_t& json_info, error_code err_code,
							const std::string& e_msg,
							value_allocator_t& json_allocator,
							value_t& out_json_info)
{
	out_json_info.SetObject();

	if (json_info.IsObject())
	{
		auto it = json_info.FindMember("id");
		if (it != json_info.MemberEnd())
		{
			out_json_info.AddMember("id", it->value, json_allocator);
		}
		else
		{
			out_json_info.AddMember("id", 1U, json_allocator); // try with id=1
		}
	}
	else
	{
		out_json_info.AddMember("id", 1U, json_allocator); // try with id=1
	}

	value_t null_value(rapidjson::kNullType);
	out_json_info.AddMember("result", null_value, json_allocator);
	value_t err_msg(e_msg.c_str(), e_msg.size(), json_allocator);
	value_t err_obj(rapidjson::kObjectType);
	err_obj.AddMember("code", err_code, json_allocator);
	err_obj.AddMember("message", err_msg, json_allocator);
	out_json_info.AddMember("error", err_obj, json_allocator);
}

void check_received_json_info(const value_t& json_info)
{
	if(!json_info.HasMember("id") || json_info["id"].IsObject() || json_info["id"].IsArray())
	{
		throw error_info(INVALID_REQUEST, "Invalid JSON-RPC request. Field 'id' is wrong type or missing");
	}
	if(!json_info.HasMember("method") || !json_info["method"].IsString())
	{
		throw error_info(INVALID_REQUEST, "Invalid JSON-RPC request. Field 'method' is wrong type or missing");
	}
	if(!json_info.HasMember("params") || !json_info["params"].IsArray())
	{
		throw error_info(INVALID_REQUEST, "Invalid JSON-RPC request. Field 'params' is wrong type or missing");
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

handler::~handler()
{
	std::for_each(m_callbacks.cbegin(), m_callbacks.cend(), [](const callbacks_t::value_type& v) { X3ME_DELETE(v.second); });
}

bool handler::process(x3me::types::string_t& received_data, x3me::types::string_t& out_data, x3me::types::string_t* out_err_ptr)
{
	document_t out_json_info;
	
	bool result = true;
	document_t received_json_info;
	try
	{
		if(received_json_info.ParseInsitu<0>(&received_data[0]).HasParseError())
		{
			throw error_info(INVALID_JSON, 
                    GetParseError_En(received_json_info.GetParseError()));
		}

		if(received_json_info.IsObject())
		{
			process(received_json_info, out_json_info.GetAllocator(), out_json_info);
		}
		//else if(received_json_info.IsArray()) 
		//{
		//	out_json_info.SetArray();

		//	auto count_entries = received_json_info.Size();
		//	for(decltype(count_entries) i = 0; i < count_entries; ++i)
		//	{
		//		value_t out_json;
		//		process(received_json_info[i], out_json_info.GetAllocator(), out_json);
		//		if(!out_json.IsNull()) // it is not a notification, add to array of responses
		//		{
		//			out_json_info.PushBack(out_json, out_json_info.GetAllocator());
		//		}
		//	}
		//	if(out_json_info.Empty())
		//	{
		//		out_json_info.SetNull();
		//	}
		//}
		else
		{
			throw error_info(INVALID_REQUEST, "Root element must be object");
		}			
	}
	catch(error_info& err) 
	{
		prepare_error_response(received_json_info, err.code, err.msg, out_json_info.GetAllocator(), out_json_info);
		if(out_err_ptr)
		{
			*out_err_ptr = err.msg;
		}		
		result = false;
	}
	catch(std::exception& err)
	{
		prepare_error_response(received_json_info, INTERNAL_ERROR, err.what(), out_json_info.GetAllocator(), out_json_info);
		if(out_err_ptr)
		{
			*out_err_ptr = err.what();
		}		
		result = false;
	}

	if(!out_json_info.IsNull()) // do not answer on notifications
	{
		out_data.reserve(4096);
		x3me::json::json_to_string(out_json_info, std::back_inserter(out_data));
	}

	return result;
}

void handler::process(value_t& in_json, value_allocator_t& json_allocator, value_t& out_json)
{
	check_received_json_info(in_json);
	
	auto method_name	= in_json["method"].GetString();
	auto found_method	= m_callbacks.find(method_name);
	if(found_method == m_callbacks.end())
	{
        x3me::utilities::string_builder_128 sb;
        sb << "Not found RPC method '" << method_name << "'";
		throw error_info(METHOD_NOT_FOUND, sb.to_string());
	}

	callback_ptr_t& method = found_method->second;

	value_t out_info;
	out_value ov(out_info, json_allocator);
	
	method->exec(in_json["params"], ov);

	if(!ov.value.IsNull()) // do not answer on notifications
	{	
		value_t null_value(rapidjson::kNullType);
		out_json.SetObject();
		out_json.AddMember("result", out_info, json_allocator);
		out_json.AddMember("id", in_json["id"], json_allocator);
	}
}

} // namespace rpc
} // namespace json
} // namesapce x3me
