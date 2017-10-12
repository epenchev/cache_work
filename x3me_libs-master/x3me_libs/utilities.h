#pragma once

#include <boost/algorithm/hex.hpp>
#include "encode.h"

namespace x3me
{
namespace utilities
{

template<typename OutValueType, OutValueType Func(const char*, char**, int)>
inline bool string_to_number(const char* str, size_t length, OutValueType& out_value)
{
	char* end_ptr			= nullptr;
	out_value				= Func(str, &end_ptr, 10);
	if(!end_ptr || (static_cast<size_t>(end_ptr - str) != length)) // whole string should be valid number, otherwise error
	{
		return false;
	}
	return true;
}
template<typename OutValueType, OutValueType Func(const char*, char**, int)>
inline bool string_to_number(const std::string& s, OutValueType& out_value)
{
	return string_to_number<OutValueType, Func>(s.c_str(), s.length(), out_value);
}

inline bool string_to_signed_number64(const char* str, size_t length, long long& out_value)
{
#ifdef X3ME_MICROSOFT_COMPILER
	return string_to_number<long long, ::_strtoi64>(str, length, out_value);
#else /*X3ME_GNU_COMPILER*/
	return string_to_number<long long, ::strtoll>(str, length, out_value);
#endif
}
inline bool string_to_signed_number64(const std::string& s, long long& out_value)
{
	return string_to_signed_number64(s.c_str(), s.length(), out_value);
}

inline bool string_to_unsigned_number64(const char* str, size_t length, unsigned long long& out_value)
{
#ifdef X3ME_MICROSOFT_COMPILER
	return string_to_number<unsigned long long, ::_strtoui64>(str, length, out_value);
#else /*X3ME_GNU_COMPILER*/
	return string_to_number<unsigned long long, ::strtoull>(str, length, out_value);
#endif
}
inline bool string_to_unsigned_number64(const std::string& s, unsigned long long& out_value)
{
	return string_to_unsigned_number64(s.c_str(), s.length(), out_value);
}

template<typename T>
inline void set_bit_by_pos(T& n, size_t bit_pos)
{
	n |= (static_cast<T>(1) << bit_pos);
}
template<typename T>
inline bool bit_set_by_pos(const T& n, size_t bit_pos)
{
	return !!(n & (static_cast<T>(1) << bit_pos));
}
template<typename T1, typename T2>
inline void set_bits(T1& n, const T2& bits)
{
	n |= bits;
}
template<typename T1, typename T2>
inline void reset_bits(T1& n, const T2& bits)
{
	n &= ~bits;
}
template<typename T1, typename T2, typename T3>
inline void reset_set_bits(T1& n, const T2& reset_bits, const T3& set_bits)
{
	n = ((n & ~reset_bits) | set_bits);
}
template<typename T1, typename T2>
inline bool all_of_bits_set(const T1& n, const T2& bits)
{
	return ((n & bits) == bits);
}
template<typename T1, typename T2>
inline bool any_of_bits_set(const T1& n, const T2& bits)
{
	return ((n & bits) != 0);
}
template<typename T1, typename T2>
inline bool some_of_bits_not_set(const T1& n, const T2& bits)
{
	return ((n & bits) != bits);
}
template<typename T1, typename T2>
inline bool none_of_bits_set(const T1& n, const T2& bits)
{
	return ((n & bits) == 0);
}
template<typename T1, typename T2>
inline T1 mask_bits(const T1& n, const T2& mask)
{
	return (n & mask);
}

struct text_data_printer
{
	const char*	m_beg;
	size_t		m_size;
	text_data_printer(const char* beg, size_t size) : m_beg(beg), m_size(size) {}
	friend std::ostream& operator <<(std::ostream& os, const text_data_printer& tdp)
    {
        encode::encode_ascii_control_codes(tdp.m_beg, tdp.m_size, 
                std::ostream_iterator<char>(os));
        return os;
    }
};

struct hex_data_printer
{
	const std::string& info; // TODO_ Do it with template along with hex_encode function
	explicit hex_data_printer(const std::string& i) : info(i) {}
	friend std::ostream& operator <<(std::ostream& os, const hex_data_printer& hdp)
	{
		typedef std::ostream_iterator<std::string::value_type> os_it_t;
		boost::algorithm::hex(hdp.info.cbegin(), hdp.info.cend(), os_it_t(os));
		return os;
	}
};

template<typename It>
class hex_data_printer2 // TODO_ Only this version should remains, the other should be removed
{
	It beg, end;
public:
	hex_data_printer2(It b, It e) : beg(b), end(e) {}
	friend std::ostream& operator <<(std::ostream& os, const hex_data_printer2& hdp)
	{
		typedef std::ostream_iterator<typename std::iterator_traits<It>::value_type> os_it_t;
		boost::algorithm::hex(hdp.beg, hdp.end, os_it_t(os));
		return os;
	}
};
template<typename It>
inline hex_data_printer2<It> print_hex_data(It beg, It end)
{
	return hex_data_printer2<It>(beg, end);
}
template<typename It>
inline hex_data_printer2<It> print_hex_data(It beg, size_t size)
{
	return hex_data_printer2<It>(beg, beg+size);
}
template<typename Cont>
inline auto print_hex_data(const Cont& c) -> hex_data_printer2<decltype(std::begin(c))>
{
	return hex_data_printer2<decltype(std::begin(c))>(std::begin(c), std::end(c));
}

template<typename It>
struct binary_data_printer
{
	It		m_beg;
	It		m_end;
	size_t	m_size;

	binary_data_printer(It beg, It end) : m_beg(beg), m_end(end), m_size(std::distance(beg, end)) {}

	friend std::ostream& operator <<(std::ostream& os, const binary_data_printer& bdp)
	{
		os << bdp.m_size << ':';
		os << std::setfill('0');
		std::for_each(bdp.m_beg, bdp.m_end, [&os](char c)
		{
			os << std::hex << std::setw(2) << static_cast<unsigned short>(static_cast<unsigned char>(c));
		});
		os << std::dec;
		return os;
	}
};
template<typename It>
inline binary_data_printer<It> print_binary_data(It beg, It end)
{
	return binary_data_printer<It>(beg, end);
}
template<typename It>
inline binary_data_printer<It> print_binary_data(It beg, size_t size)
{
	return binary_data_printer<It>(beg, beg+size);
}

} // namespace utilities
} // namespace x3me
