#pragma once

#include "json_rpc_common.h"
#include "json_rpc_handler.h"
#include "string_builder.h"
#include "utilities.h"

namespace x3me
{
namespace json
{
namespace rpc
{

typedef std::function<void (const types::string_t& error_info)> on_server_error_func_t;

template<typename Session>
struct server_session
{
	typedef Session	session_t;
	typedef types::unique_array_ptr_t<char>::type recv_buffer_t;
	enum { recv_buffer_size = 512 };
	recv_buffer_t	recv_buffer;
	types::string_t	recv_data;
	types::string_t	send_data;
	session_t		session;

	explicit server_session(session_t&& session) : recv_buffer(X3ME_NEW_ARRAY(char, recv_buffer_size)), session(std::forward<session_t>(session)) {}

private:
	server_session(const server_session&);
	server_session& operator =(const server_session&);
};

template<typename Acceptor>
class server
{
	enum parse_request_result
	{
		parse_error = -1,
		complete_request,
		non_complete_request,		
	};

	typedef Acceptor							acceptor_t;
	typedef typename acceptor_t::io_error_t		io_error_t;
	typedef typename acceptor_t::io_error2_t	io_error2_t;
	typedef typename acceptor_t::system_error_t	system_error_t;

	typedef server_session<typename  acceptor_t::session_t> session_t;
	typedef std::shared_ptr<session_t>						session_ptr_t;

	on_server_error_func_t	m_on_error_func;
	acceptor_t				m_acceptor;
	handler					m_handler;

public:
	explicit server(Acceptor&& acceptor) : m_acceptor(std::forward<acceptor_t>(acceptor)) {}

	// TODO_ variadic templates can be used
	void add_callback(const char* name, std::function<void (out_value&)>&& callback)
	{
		m_handler.add_callback(name, std::move(callback));
	}
	template<typename T>
	void add_callback(const char* name, std::function<void (const T&, out_value&)>&& callback)
	{
		m_handler.add_callback(name, std::move(callback));
	}
	template<typename T1, typename T2>
	void add_callback(const char* name, std::function<void (const T1&, const T2&, out_value&)>&& callback)
	{
		m_handler.add_callback(name, std::move(callback));
	}
	template<typename T1, typename T2, typename T3>
	void add_callback(const char* name, std::function<void (const T1&, const T2&, const T3&, out_value&)>&& callback)
	{
		m_handler.add_callback(name, std::move(callback));
	}
	template<typename Handler>
	bool start(const types::string_t& addr, unsigned short port, Handler&& on_error_func)
	{
		m_on_error_func	= std::forward<Handler>(on_error_func);
		if(m_acceptor.start(addr, port))
		{
			async_accept();
			return true;
		}
		return false;
	}
	void stop()
	{
		m_acceptor.close();
	}

private:
	void async_accept()
	{
		auto session_ptr = X3ME_ALLOCATE_SHARED(session_t, m_acceptor.create_session());
		m_acceptor.async_accept(session_ptr->session.get_socket(), [this, session_ptr](const system_error_t& err)
		{
			if(!err)
			{
				this->async_recv(session_ptr);
			}
			else
			{
				this->m_on_error_func("accept err. " + err.message());
			}
			this->async_accept();
		});
	}
	void async_recv(const session_ptr_t& session_ptr)
	{
		auto& recv_buffer = session_ptr->recv_buffer;
		session_ptr->session.async_recv(recv_buffer.get(), session_t::recv_buffer_size, [this, session_ptr](const system_error_t& err, size_t recv_bytes)
		{
			if(!err)
			{
				session_ptr->recv_data.append(session_ptr->recv_buffer.get(), recv_bytes);
				types::string_t json_request, err_info;
				switch(this->parse_request(session_ptr->recv_data, json_request, err_info))
				{
				case complete_request:
					session_ptr->send_data = "HTTP/1.0 200 OK\r\ncontent-type: application/json\r\n\r\n"; // response data will be appended to this
					if(this->m_handler.process(json_request, session_ptr->send_data, &err_info))
					{
						this->async_send(session_ptr);
					}
					else
					{
						this->async_send(session_ptr);
						this->m_on_error_func(err_info);
					}
					break;
				case non_complete_request:
					this->async_recv(session_ptr);
					break;
				case parse_error:
					this->m_on_error_func(err_info);
					break;
				default:
					assert(false);
					break;
				}
			}
			else
			{
				this->m_on_error_func("recv err. " + err.message());
			}
		});
	}
	void async_send(const session_ptr_t& session_ptr)
	{
		auto& send_data = session_ptr->send_data;
		session_ptr->session.async_send(send_data.c_str(), send_data.size(),
				[this, session_ptr](const system_error_t& err, size_t /*sent_bytes*/)
		{
			if(!!err) // MSVC gives conversion warning without !!
			{
				this->m_on_error_func("send err. " + err.message());
			}
			// HACK discard all not read data from socket recv buffer otherwise socket close will send RST to the other side and this will breaks not finished receiving
			// this hack is only needed because bug in php which sends \r\n\r\n after POST request message body.
			// The proper decision was to workaround the php bug in the php code, but ...
			session_ptr->session.end_session();
		});
	}
	
	template<size_t label_size>
	static bool get_string_between_label_and_new_line(const types::string_t& in, const char (&label)[label_size], types::string_t& out)
	{
		auto label_pos = in.find(label);
		if(label_pos != types::string_t::npos)
		{
			auto out_beg_pos	= label_pos + (label_size - 1);
			auto n_pos			= in.find('\n', out_beg_pos);
			auto rn_pos			= in.find("\r\n", out_beg_pos);
			if((rn_pos != types::string_t::npos) && (n_pos != types::string_t::npos))
			{
				auto out_end_pos = std::min(rn_pos, n_pos);
				out = std::move(in.substr(out_beg_pos, out_end_pos-out_beg_pos));
				return true;
			}
			else if(rn_pos != types::string_t::npos)
			{
				out = std::move(in.substr(out_beg_pos, rn_pos-out_beg_pos));
				return true;
			}
			else if(n_pos != types::string_t::npos)
			{
				out = std::move(in.substr(out_beg_pos, n_pos-out_beg_pos));
				return true;
			}
		}
		return false;
	}
	static parse_request_result parse_request(const types::string_t& recv_data, types::string_t& out_json_request, types::string_t& out_err_info)
	{
		enum { req_max_size = 2048 };
		const size_t req_size = recv_data.size();
		if(req_size > req_max_size)
		{
			out_err_info = (utilities::string_builder_128() << 
                    "too long request. max permitted size " << req_max_size << 
                    ". current request size " << req_size).to_string();
			return parse_error;
		}
		size_t header_end_pos = recv_data.find("\r\n\r\n");
		size_t data_start_pos = header_end_pos + 4;
		if(header_end_pos == types::string_t::npos)
		{
			header_end_pos = recv_data.find("\n\n");
			data_start_pos = header_end_pos + 2;		
		}
		if(header_end_pos == types::string_t::npos)
		{
			if(recv_data.find(" HTTP") == types::string_t::npos)
			{
				out_err_info = "non HTTP request";
				return parse_error;
			}
			return non_complete_request;
		}
		auto data_size = req_size - data_start_pos;
		types::string_t content;
		if(get_string_between_label_and_new_line(recv_data, "Content-Length: ", content))
		{
			long long length = 0;
			if(!utilities::string_to_signed_number64(content, length) || (length <= 0))
			{
				out_err_info = "invalid Content-Length";
				return parse_error;
			}
			if(data_size < static_cast<unsigned long long>(length))
			{
				return non_complete_request;
			}
		}
		content.clear();
		if(get_string_between_label_and_new_line(recv_data, "Content-type: ", content) && (content != "application/json"))
		{
			out_err_info = "invalid Content-type: " + content;
			return parse_error;
		}
		out_json_request = std::move(recv_data.substr(data_start_pos, data_size));
		return complete_request;
	}
};

} // namespace rpc
} // namespace json
} // namespace x3me
