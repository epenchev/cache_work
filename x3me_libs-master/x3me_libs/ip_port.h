#pragma once

struct ip_port // ipV4
{
	typedef uint32_t	ip_t;
	typedef uint16_t	port_t;

	enum 
	{ 
		not_set			= 0,
		count_ip_bytes	= sizeof(ip_t),
		count_ipp_bytes = sizeof(ip_t) + sizeof(port_t),
	};

	ip_t	ip;
	port_t	port;

	ip_port() : ip(not_set), port(not_set) {}
	ip_port(ip_t ip_, port_t port_) : ip(ip_), port(port_) {}

	unsigned long long get_as_ulonglong() const
	{
		union
		{
			unsigned long long value;
			struct
			{
				ip_t	ip;
				port_t	port;
			} ipp; 
		} hasher;
		hasher.value	= 0;
		hasher.ipp.ip	= ip;
		hasher.ipp.port	= port;
		return hasher.value;
	}

	void set(ip_t ip_, port_t port_)
	{
		ip		= ip_;
		port	= port_;
	}

	void reset()
	{
		ip		= 0;
		port	= 0;
	}

	bool is_set() const
	{
		return ((ip != not_set) && (port != not_set));
	}

	template<class Archive>
	void serialize(Archive& ar, const unsigned int/* version*/)
	{
		ar & ip;
		ar & port;
	}

	inline static ip_port get_ipp_from_network_order_bytes(const char* buffer) // the outside must guarantee that buffer has at least six bytes length
	{
		ip_port ipp;
		unsigned char* p_ip		= reinterpret_cast<unsigned char*>(&ipp.ip);
		unsigned char* p_port	= reinterpret_cast<unsigned char*>(&ipp.port);
		p_ip[3]		= buffer[0];
		p_ip[2]		= buffer[1];
		p_ip[1]		= buffer[2];
		p_ip[0]		= buffer[3];
		p_port[1]	= buffer[4];
		p_port[0]	= buffer[5];
		return ipp;
	}

	template<size_t buffer_size>
	static char* get_ipp_as_bytes_in_network_order(const ip_port& ipp, std::array<char, buffer_size>& buffer)
	{
		static_assert(buffer_size >= 6, "function needs of at least 6 byte buffer");
		const unsigned char* p_ip	= reinterpret_cast<const unsigned char*>(&ipp.ip);
		const unsigned char* p_port = reinterpret_cast<const unsigned char*>(&ipp.port);
		buffer[0] = p_ip[3];
		buffer[1] = p_ip[2];
		buffer[2] = p_ip[1];
		buffer[3] = p_ip[0];
		buffer[4] = p_port[1];
		buffer[5] = p_port[0];
		return buffer.data();
	}

	template<size_t buffer_size>
	static const char* get_ip_as_dotted_string(const ip_port::ip_t& ip, std::array<char, buffer_size>& buff)
	{
		static_assert(buffer_size >= 16, "function needs of at least 16 byte buffer");
		const unsigned char* cip = reinterpret_cast<const unsigned char*>(&ip);
		auto r = snprintf(buff.data(), buff.size(), "%hhu.%hhu.%hhu.%hhu", 
                        cip[3], cip[2], cip[1], cip[0]);
        assert((r > 0) && (r < static_cast<int>(buffer_size)));
		return buff.data();
	}

	template<size_t buffer_size>
	static const char* to_string(const ip_port& ipp, std::array<char, buffer_size>& buff)
	{
		// 22 will be enough - 15 bytes for ip, 1 byte ':', 5 bytes for port and 1 byte zero terminator
		// the function will require 24 bytes for good alignement
		static_assert(buffer_size >= 24, "function needs of at least 24 byte buffer");
		const unsigned char* cip = reinterpret_cast<const unsigned char*>(&ipp.ip);
		int r = snprintf(buff.data(), buff.size(), "%hhu.%hhu.%hhu.%hhu:%hu", 
                        cip[3], cip[2], cip[1], cip[0], ipp.port);
        assert((r > 0) && (r < static_cast<int>(buffer_size)));
		return buff.data();
	}
};

typedef std::vector<ip_port::ip_t> ip_array_t;
typedef std::vector<ip_port>	   ipport_array_t;

inline std::ostream& operator <<(std::ostream& os, const ip_port& ipp)
{
	std::array<char, 32> buff;
	os << ip_port::to_string(ipp, buff);
	return os;
}

inline bool operator ==(const ip_port& ipp1, const ip_port& ipp2)
{
	return (ipp1.ip == ipp2.ip) && (ipp1.port == ipp2.port);
}

inline bool operator !=(const ip_port& ipp1, const ip_port& ipp2)
{
	return (ipp1.ip != ipp2.ip) || (ipp1.port != ipp2.port);
}

inline bool operator <(const ip_port& ipp1, const ip_port& ipp2)
{
	return (ipp1.get_as_ulonglong() < ipp2.get_as_ulonglong());
}

namespace std
{
	template<> struct hash<ip_port>
	{
		inline size_t operator ()(const ip_port& ipp) const
		{
			return std::hash<unsigned long long>()(ipp.get_as_ulonglong());
		}
	};
}
