#include "types.h"
#include "url_encoding.h"

namespace x3me
{
namespace urlencoding
{

x3me::types::string_t url_encode(const x3me::types::string_t& s)
{
	// Only 0-9, a-z, A-Z, '.', '-', '_' and '~' are safe and should not be encoded
	static const char SAFE[256] =
	{
		/*      0 1 2 3  4 5 6 7  8 9 A B  C D E F */
		/* 0 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
		/* 1 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
		/* 2 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,1,0, // '-', '.'
		/* 3 */ 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0,

		/* 4 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1, // A-O
		/* 5 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,1, // P-Z, '_'
		/* 6 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1, // a-o
		/* 7 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,1,0, // p-z, '~'

		/* 8 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
		/* 9 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
		/* A */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
		/* B */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

		/* C */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
		/* D */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
		/* E */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
		/* F */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
	};
	static const char DEC2HEX[16+1] = "0123456789ABCDEF";

	const size_t src_size = s.size();
	// TODO_ reserve and push_back
	x3me::types::string_t result(3*src_size, 0); // 3*src_size - work with worst case when all symbols needs to be escaped
	char* const dst_begin				= &result[0];
	char* dst							= dst_begin;
	const unsigned char* src			= reinterpret_cast<const unsigned char*>(s.data());
	const unsigned char* const src_end	= src + src_size;

	for(; src < src_end; ++src)
	{
		unsigned char ch = *src;
		if(SAFE[ch])
		{
			*dst++ = ch;
		}
		else // escape this char
		{
			*dst++ = '%';
			*dst++ = DEC2HEX[ch >> 4];
			*dst++ = DEC2HEX[ch & 0x0F];
		}
	}

	size_t dst_size = dst - dst_begin;
	result.resize(dst_size); // shrink to fit

	return result;
}

void url_decode_impl(const x3me::types::string_t& s, x3me::types::string_t& result) // result must have enough size
{
	static const char nh = -1; // nh - no hex
	static const char HEX2DEC[256] = 
	{
		/*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
		/* 0 */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* 1 */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* 2 */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,nh,nh, nh,nh,nh,nh,

		/* 4 */ nh,10,11,12, 13,14,15,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* 5 */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* 6 */ nh,10,11,12, 13,14,15,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* 7 */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,

		/* 8 */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* 9 */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* A */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* B */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,

		/* C */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* D */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* E */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh,
		/* F */ nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh, nh,nh,nh,nh
	};

	const size_t src_size							= s.size();
	char* const dst_begin							= &result[0]; // TODO_ do it with push_back
	char* dst										= dst_begin;
	const unsigned char* src						= reinterpret_cast<const unsigned char*>(s.data());
	const unsigned char* const src_end				= src + src_size;
	const unsigned char* const src_last_decodable	= src_end - 2; // last decodable '%' 

	while(src < src_last_decodable)
	{
		if(src[0] == '%')
		{
			char ch1 = HEX2DEC[src[1]];
			char ch2 = HEX2DEC[src[2]];
			if((ch1 != nh) && (ch2 != nh))
			{
				*dst++ = ((ch1 << 4) | ch2);
				src += 3;
				continue;
			}
		}
		*dst++ = *src++;
	}
	while(src < src_end) // decode last 2 characters if needed
	{
		*dst++ = *src++;
	}

	size_t dst_size = dst - dst_begin;
	result.resize(dst_size); // shrink to fit
}

x3me::types::string_t url_decode(const x3me::types::string_t& s)
{
	x3me::types::string_t result(s.size(), 0);
	url_decode_impl(s, result);
	return result;
}

void url_decode_inplace(x3me::types::string_t& s)
{
	url_decode_impl(s, s);
}

} // namespace urlencoding
} // namespace x3me
