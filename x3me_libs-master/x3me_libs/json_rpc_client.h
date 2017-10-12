#pragma once

#include "json_rpc_common.h"
#include "string_builder.h"

namespace x3me
{
namespace json
{
namespace rpc
{
typedef std::function<void (x3me::json::value_t&)>																on_response_func_t;
typedef std::function<void (const x3me::types::string_t& method_name, const x3me::types::string_t& error_info)>	on_client_error_func_t;

// TODO_ 
// in the current moment client doesn't support notifications
// think for smarter way (if such exists) to hide communication part, with smaller amount of outside code
template<typename Session>
class client
{
	typedef Session								session_t;
	typedef typename session_t::io_error_t		io_error_t;
	typedef typename session_t::io_error2_t		io_error2_t;
	typedef typename session_t::system_error_t	system_error_t;

	typedef x3me::types::unique_array_ptr_t<char>::type recv_buffer_t;

	enum { recv_buffer_size = 4096 };

	// TODO_ in the current moment this client doesn't support specific path in the URL address
	bool					m_is_connected;
	unsigned short			m_port;
	recv_buffer_t			m_recv_buffer;
	x3me::types::string_t	m_address;		
	x3me::types::string_t	m_send_data;
	x3me::types::string_t	m_recv_data;
	x3me::types::string_t	m_current_method_name;
	on_response_func_t		m_on_response_func;
	on_client_error_func_t			m_on_error_func;
	session_t				m_session;

public:
	explicit client(session_t&& session) : m_is_connected(false), m_port(0), m_recv_buffer(X3ME_NEW_ARRAY(char, recv_buffer_size)), m_session(std::forward<session_t>(session)) {}
	template<typename Handler>
	void start(const types::string_t& address, unsigned short port, Handler&& on_error_func)
	{
		m_address		= address;
		m_port			= port;
		m_on_error_func	= std::forward<Handler>(on_error_func);
		m_send_data.clear(); // to enable new send
		m_recv_data.clear();
		m_session.async_connect(address, port, std::bind(&client::on_connect, this, std::placeholders::_1/*error*/));
	}
	void stop()
	{
		m_session.close();
		m_is_connected = false;
	}
	// NOTE_ the class will support only nameless json rpc calls
	// if more params are needed do it with boost pp for windows and variadic templates for linux
#define DEFINE_PARAMS_ARRAY(size) document_t params; params.SetArray(); params.Reserve(size, params.GetAllocator())
#define ADD_PARAM(param) add_to_array(param, params, params.GetAllocator())
#define SEND_REQUEST send_request(json_rpc_method, params, params.GetAllocator())
	template<typename Handler>
	void call(const char* json_rpc_method, Handler&& on_response_func)
	{
		m_on_response_func = std::forward<Handler>(on_response_func);
		DEFINE_PARAMS_ARRAY(1);
		SEND_REQUEST;
	}
	template<typename Arg1, typename Handler>
	void call(const char* json_rpc_method, Arg1& arg1, Handler&& on_response_func)
	{
		m_on_response_func = std::forward<Handler>(on_response_func);
		DEFINE_PARAMS_ARRAY(1);
		ADD_PARAM(arg1);
		SEND_REQUEST;
	}
	template<typename Arg1, typename Arg2, typename Handler>
	void call(const char* json_rpc_method, Arg1& arg1, Arg2& arg2, Handler&& on_response_func)
	{
		m_on_response_func = std::forward<Handler>(on_response_func);
		DEFINE_PARAMS_ARRAY(2);
		ADD_PARAM(arg1);
		ADD_PARAM(arg2);
		SEND_REQUEST;
	}
	template<typename Arg1, typename Arg2, typename Arg3, typename Handler>
	void call(const char* json_rpc_method, Arg1& arg1, Arg2& arg2, Arg2& arg3, Handler&& on_response_func)
	{
		m_on_response_func = std::forward<Handler>(on_response_func);
		DEFINE_PARAMS_ARRAY(3);
		ADD_PARAM(arg1);
		ADD_PARAM(arg2);
		ADD_PARAM(arg3);
		SEND_REQUEST;
	}
#undef DEFINE_PARAMS_ARRAY
#undef ADD_PARAM
#undef SEND_REQUEST

private:
	// these three functions are perfect for "co-routine implementation"
	void on_connect(const system_error_t& err)
	{
		if(!err)
		{
			m_is_connected = true;
			if(!m_send_data.empty())
			{
				m_session.async_send(m_send_data.c_str(), m_send_data.size(), std::bind(&client::on_send, this, std::placeholders::_1/*error*/, std::placeholders::_2/*sent_bytes*/));
			}
		}
		else
		{
			x3me::utilities::string_builder_128 info;
			info << "unable to connect to " << m_address << ':' << m_port << ". " << err.message();
			m_on_error_func(m_current_method_name, info.to_string());
		}
	}
	void on_send(const system_error_t& err, size_t /*sent_bytes*/)
	{
		if(!err)
		{
			m_session.async_recv(m_recv_buffer.get(), recv_buffer_size, std::bind(&client::on_recv, this, std::placeholders::_1/*error*/, std::placeholders::_2/*recv_bytes*/));
		}
		else
		{
			x3me::utilities::string_builder_128 info;
			info << "unable to send to " << m_address << ':' << m_port << ". " << err.message();
			m_on_error_func(m_current_method_name, info.to_string());
		}
	}
	void on_recv(const system_error_t& err, size_t recv_bytes)
	{
		if(!err)
		{
			m_recv_data.append(m_recv_buffer.get(), recv_bytes);
			m_session.async_recv(m_recv_buffer.get(), recv_buffer_size, std::bind(&client::on_recv, this, std::placeholders::_1/*error*/, std::placeholders::_2/*recv_bytes*/));
		}
		else if(err == io_error2_t::eof)
		{
			// it is not very correct to read to EOF and then to check the response, but in the current moment is needed speed coding
			m_send_data.clear(); // to enable another send
			parse_received_response();
			m_recv_data.clear();
		}
		else
		{
			x3me::utilities::string_builder_128 info;
			info << "unable to recv from " << m_address << ':' << m_port << ". " << err.message();
			m_on_error_func(m_current_method_name, info.to_string());
		}
	}

	void send_request(const char* json_rpc_method, x3me::json::value_t& params, x3me::json::value_allocator_t& allocator)
	{
		// send buffer is emptied after send
		// the outside must not send, if the response from the previous is not received
		if(m_send_data.empty())
		{
			m_current_method_name = json_rpc_method;

			x3me::json::value_t result_json(rapidjson::kObjectType);
			result_json.AddMember(json::string_ref_t("method"), 
                    json::string_ref_t(json_rpc_method), allocator);
			result_json.AddMember(json::string_ref_t("params"),
                    params, allocator);
			result_json.AddMember(json::string_ref_t("id"), 
                    1, allocator); // in the current moment "parallel" json-rpc calls are not supported

			x3me::types::string_t data; data.reserve(256);
			json_to_string(result_json, std::back_inserter(data));

			x3me::utilities::string_builder_512 result; 
			result << 
				"POST / HTTP/1.0\r\n"
				"Host: " << m_address << ':' << m_port << "\r\n"
				"Content-Length: " << data.length() << "\r\n"
				"Content-type: application/json\r\n\r\n" <<
				data;

			m_send_data = result.to_string();
			if(m_is_connected)
			{
				m_session.async_send(m_send_data.c_str(), m_send_data.size(), std::bind(&client::on_send, this, std::placeholders::_1/*error*/, std::placeholders::_2/*sent_bytes*/));
			}
		}
		else
		{
			m_on_error_func(json_rpc_method, "previous call to method '" + m_current_method_name + "' is not finished yet");
		}
	}

	template<typename T>
	void add_to_array(T& value, x3me::json::value_t& arr, x3me::json::value_allocator_t& allocator)
	{
		arr.PushBack(value, allocator);
	}
	void add_to_array(const x3me::types::string_t& v, x3me::json::value_t& arr, x3me::json::value_allocator_t& allocator)
	{
		x3me::json::value_t value(v.c_str(), v.size(), allocator);
		arr.PushBack(value, allocator);
	}

	bool check_received_json_info(const value_t& json_info)
	{
		if (!json_info.IsObject())
		{
			m_on_error_func(m_current_method_name,
				"Received JSON is not an object "
				"as required in the JSON specification");
			return false;
		}
		auto err = json_info.FindMember("error");
		if (err != json_info.MemberEnd())
		{
			if (!err->value.IsObject())
			{
				m_on_error_func(m_current_method_name,
						"The error entry is not an object "
						"as required in the JSON specification");
				return false;
			}
			auto err_msg = err->value.FindMember("message");
			if (err_msg == err->value.MemberEnd())
			{
				m_on_error_func(m_current_method_name,
						"No message entry in the error object "
						"as required in the JSON specification");
				return false;
			}
			if (err_msg->value.IsString())
			{
				std::string err(err_msg->value.GetString(),
								err_msg->value.GetStringLength());
				m_on_error_func(m_current_method_name, "json error. " + err);
				return false;
			}
			else
			{
				m_on_error_func(m_current_method_name,
						"The error::message is not a string type "
						"as required in the JSON specification");
				return false;
			}
		}
		if(!json_info.HasMember("id") || !json_info["id"].IsNumber())
		{
			m_on_error_func(m_current_method_name, "invalid json received. Field 'id' is wrong type or missing");
			return false;
		}
		if(!json_info.HasMember("result"))
		{
			m_on_error_func(m_current_method_name, "invalid json received. Field 'result' is missing");
			return false;
		}
		return true;
	}
	void parse_received_response()
	{
		size_t json_start_pos = m_recv_data.find("\r\n\r\n");
		if(json_start_pos != x3me::types::string_t::npos)
		{
			json_start_pos += 4;
		}
		else
		{
			json_start_pos = m_recv_data.find("\n\n");
			if(json_start_pos != x3me::types::string_t::npos)
			{
				json_start_pos += 2;
			}
		}
		if(json_start_pos != x3me::types::string_t::npos)
		{
			document_t received_json_info;
			if(!received_json_info.ParseInsitu<0>(&m_recv_data[json_start_pos]).HasParseError())
			{
				if(check_received_json_info(received_json_info))
				{
					m_on_response_func(received_json_info["result"]);
				}
			}
			else
			{
				m_on_error_func(m_current_method_name, 
                        x3me::types::string_t("invalid json received. ") + 
                        GetParseError_En(received_json_info.GetParseError()));
			}
		}
		else
		{
			m_on_error_func(m_current_method_name, "incorrect HTTP response received. not found 'crlfcrlf' or 'lflf'");
		}
	}

private:
	client();
	client(const client&);
	client& operator =(const client);
};	
} // namespace rpc
} // namespace json
} // namespace x3me
