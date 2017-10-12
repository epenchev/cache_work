#include "types.h"
#include "bencoding.h"
#include "convert.h"
#include "utilities.h"

namespace
{
	template <typename RealObjectType, typename InputType>
	void call_destructor(InputType& obj)
	{
		reinterpret_cast<RealObjectType&>(obj).~RealObjectType();
	}
}

namespace x3me
{
namespace bencoding
{

entry::entry()
: m_type(type_undefined)
{
}

entry::entry(const entry& e)
: m_type(type_undefined)
{
	construct_copy_entry(e);
}

entry::entry(entry&& e)
: m_type(type_undefined)
{
	construct_move_entry(std::move(e));
}

entry::entry(const integer_t& integer_data)
: m_type(type_integer)
{
	X3ME_PLACEMENT_NEW(&m_data, integer_t)(integer_data);
}

entry::entry(const string_t& string_data)
: m_type(type_string)
{
	X3ME_PLACEMENT_NEW(&m_data, string_t)(string_data);
}

entry::entry(integer_t&& integer_data)
: m_type(type_integer)
{
	X3ME_PLACEMENT_NEW(&m_data, integer_t)(integer_data);
}

entry::entry(string_t&& string_data)
: m_type(type_string)
{
	X3ME_PLACEMENT_NEW(&m_data, string_t)(std::move(string_data));
}

entry::entry(list_t&& list_data)
: m_type(type_list)
{
	X3ME_PLACEMENT_NEW(&m_data, list_t)(std::move(list_data));
}

entry::entry(dictionary_t&& dictionary_data)
: m_type(type_dictionary)
{
	X3ME_PLACEMENT_NEW(&m_data, dictionary_t)(std::move(dictionary_data));
}

entry& entry::operator =(const entry& e)
{
	if(this != &e)
	{
		if(m_type == e.m_type)
		{
			// if entry is from same type we may directly copy it, internal datay layout is the same
			copy_same_type_entry(e);
		}
		else
		{
			// otherwise we must destroy the old data and copy constructing the new data
			destruct();
			construct_copy_entry(e);
		}
	}

	return *this;
}

entry& entry::operator =(entry&& e)
{
	if(this != &e)
	{
		if(m_type == e.m_type)
		{
			// if entry is from same type we may directly move it, internal datay layout is the same
			move_same_type_entry(std::move(e));
		}
		else
		{
			// otherwise we must destroy the old data and move constructing the new data
			destruct();
			construct_move_entry(std::move(e));
		}
	}

	return *this;
}

entry::~entry()
{
	destruct();
}

void entry::set_integer(integer_t&& i)
{
	set_data<type_integer, integer_t>(std::move(i));
}

void entry::set_string(string_t&& s)
{
	set_data<type_string, string_t>(std::move(s));
}

void entry::set_list(list_t&& l)
{
	set_data<type_list, list_t>(std::move(l));
}

void entry::set_dictionary(dictionary_t&& d)
{
	set_data<type_dictionary, dictionary_t>(std::move(d));
}

void entry::destruct()
{
	switch(m_type)
	{
	case type_integer:		call_destructor<integer_t>(m_data);		break;
	case type_string:		call_destructor<string_t>(m_data);		break;
	case type_list:			call_destructor<list_t>(m_data);		break;
	case type_dictionary:	call_destructor<dictionary_t>(m_data);	break;
	default:				assert(m_type == type_undefined);		break;
	}
	m_type = type_undefined;
}

template<typename DataType, typename StorageType>
void construct_copy(StorageType& result_data, const StorageType& in_data)
{
	X3ME_PLACEMENT_NEW(&result_data, DataType)(reinterpret_cast<const DataType&>(in_data));
}

void entry::construct_copy_entry(const entry& e)
{
	switch(e.m_type)
	{
	case type_integer:		construct_copy<integer_t>(m_data, e.m_data);	break;
	case type_string:		construct_copy<string_t>(m_data, e.m_data);		break;
	case type_list:			construct_copy<list_t>(m_data, e.m_data);		break;
	case type_dictionary:	construct_copy<dictionary_t>(m_data, e.m_data);	break;
	default:				assert(m_type == type_undefined);				break;
	}
	m_type = e.m_type;
}

template<typename DataType, typename StorageType>
void construct_move(StorageType& result_data, StorageType& in_data)
{
	X3ME_PLACEMENT_NEW(&result_data, DataType)(std::move(reinterpret_cast<DataType&>(in_data)));
}

void entry::construct_move_entry(entry&& e)
{
	switch(e.m_type)
	{
	case type_integer:		construct_move<integer_t>(m_data, e.m_data);	break;
	case type_string:		construct_move<string_t>(m_data, e.m_data);		break;
	case type_list:			construct_move<list_t>(m_data, e.m_data);		break;
	case type_dictionary:	construct_move<dictionary_t>(m_data, e.m_data);	break;
	default:				assert(m_type == type_undefined);				break;
	}
	m_type = e.m_type;
}

template<typename DataType, typename StorageType>
void copy_same_type(StorageType& result_data, const StorageType& in_data)
{
	reinterpret_cast<DataType&>(result_data) = reinterpret_cast<const DataType&>(in_data);
}

void entry::copy_same_type_entry(const entry& e)
{
	switch(e.m_type)
	{
	case type_integer:		copy_same_type<integer_t>(m_data, e.m_data);	break;
	case type_string:		copy_same_type<string_t>(m_data, e.m_data);		break;
	case type_list:			copy_same_type<list_t>(m_data, e.m_data);		break;
	case type_dictionary:	copy_same_type<dictionary_t>(m_data, e.m_data);	break;
	default:				assert(m_type == type_undefined);				break;
	}
}

template<typename DataType, typename StorageType>
void move_same_type(StorageType& result_data, StorageType& in_data)
{
	reinterpret_cast<DataType&>(result_data) = std::move(reinterpret_cast<DataType&>(in_data));
}

void entry::move_same_type_entry(entry&& e)
{
	switch(e.m_type)
	{
	case type_integer:		move_same_type<integer_t>(m_data, e.m_data);	break;
	case type_string:		move_same_type<string_t>(m_data, e.m_data);		break;
	case type_list:			move_same_type<list_t>(m_data, e.m_data);		break;
	case type_dictionary:	move_same_type<dictionary_t>(m_data, e.m_data); break;
	default:				assert(m_type == type_undefined);				break;
	}
}

template<int type_of_entry, typename T>
void entry::set_data(T&& data)
{
	if(m_type == type_of_entry)
	{
		// if entry is from same type we may directly move it, internal data layout is the same
		reinterpret_cast<T&>(m_data) = std::move(data);
	}
	else
	{
		// otherwise we must destroy the old data and move constructing the new data
		destruct();
		X3ME_PLACEMENT_NEW(&m_data, T)(std::move(data));
		m_type = static_cast<entry_type>(type_of_entry);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::string::const_iterator		data_iterator_t;
typedef std::string::value_type			data_t;

enum {max_recursion_level = 100};

std::string read_until(data_iterator_t& it, data_iterator_t end_it, data_t end_token)
{
	std::string result;
	for(; it != end_it; ++it)
	{
		const data_t& token = *it;
		
		if(token == end_token)
		{
			break;
		}

		result += token;
	}
	return result;
}

bdecode_result bdecode_recursive(data_iterator_t& it, data_iterator_t end_it, size_t recusion_level, entry& result)
{
	if(++recusion_level >= max_recursion_level)
	{
		return bdecode_err_recursion_level;
	}

	if(it != end_it)
	{
		switch(*it)
		{
		case 'i': // handle integer type
			{
				++it; // move after i
				std::string string_value = read_until(it, end_it, 'e');
				if(it == end_it)
				{
					return bdecode_err_not_terminated_integer;
				}
				++it; // move after e
				long long value = 0;
				if(!x3me::utilities::string_to_signed_number64(string_value, value))
				{
					return bdecode_err_broken_integer;
				}
				result.set_integer(std::move(value));
			}
			break;

		case 'l': // handle list type
			{
				++it; // move after l
				entry::list_t entry_list;
				while(*it != 'e')
				{
					entry_list.push_back(entry());
					bdecode_result res = bdecode_recursive(it, end_it, recusion_level, entry_list.back());
					if(res != bdecode_ok)
					{
						return res;
					}
					if(it == end_it)
					{
						return bdecode_err_not_terminated_list;
					}
				}
				result.set_list(std::move(entry_list));
				++it; // move after e
			}
			break;

		case 'd': // handle dictionary type
			{
				++it; // move after d
				entry::dictionary_t entry_dictionary;
				while(*it != 'e')
				{
					entry key;
					bdecode_result res = bdecode_recursive(it, end_it, recusion_level, key);
					if(res != bdecode_ok)
					{
						return res;
					}
					if(key.get_type() != entry::type_string)
					{
						return bdecode_err_dictionary_key_not_string;
					}
					// operator [] will return empty entry that will be filled inside bdecode_recursive
					res = bdecode_recursive(it, end_it, recusion_level, entry_dictionary[*key.get_string()]);
					if(res != bdecode_ok)
					{
						return res;
					}
					if(it == end_it)
					{
						return bdecode_err_not_terminated_dictionary;
					}
				}
				result.set_dictionary(std::move(entry_dictionary));
				++it; // move after e
			}
			break;

		default: // handle string type
			{
				std::string string_length = read_until(it, end_it, ':');
				if(it == end_it)
				{
					return bdecode_err_string_length_delimiter_not_present;
				}
				++it; // move after :
				long length = 0;
				if(!x3me::utilities::string_to_number<long, ::strtol>(string_length, length))
				{
					return bdecode_err_broken_string_length;
				}
				if(length < 0)
				{
					return bdecode_err_negative_string_length;
				}
				long symbols_to_end = static_cast<long>(std::distance(it, end_it));
				if(symbols_to_end < length)
				{
					return bdecode_err_shortened_string;
				}

				data_iterator_t string_end_it = it;
				std::advance(string_end_it, length);
				result.set_string(std::string(it, string_end_it));
				it = string_end_it;
			}
			break;
		}
	}

	return bdecode_ok;
}

bencode_result bencode_recursive(const entry& e, size_t recusion_level, std::string& result)
{
	if(++recusion_level >= max_recursion_level)
	{
		return bencode_err_recursion_level;
	}

	char buffer[32]; // max number of digits in string representation of 64 bit integer is 20

	switch(e.get_type())
	{
	case entry::type_integer:
		{
			x3me::convert::int_to_str_l(*e.get_integer(), buffer);
			result += 'i';
			result += buffer;
			result += 'e';
		}
		break;
	case entry::type_string:
		{
			const std::string* s = e.get_string();
			x3me::convert::int_to_str_l(s->length(), buffer);
			result += buffer;
			result += ':';
			result += *s;
		}
		break;
	case entry::type_list:
		{
			const entry::list_t* l = e.get_list();
			result += 'l';
			for(entry::list_t::const_iterator it = l->begin(); it != l->end(); ++it)
			{
				bencode_result res = bencode_recursive(*it, recusion_level, result);
				if(res != bencode_ok)
				{
					return res;
				}
			}
			result += 'e';
		}
		break;
	case entry::type_dictionary:
		{
			const entry::dictionary_t* d = e.get_dictionary();
			result += 'd';
			for(entry::dictionary_t::const_iterator it = d->begin(); it != d->end(); ++it)
			{
				x3me::convert::int_to_str_l(it->first.length(), buffer);
				result += buffer;
				result += ':';
				result += it->first;
				bencode_result res = bencode_recursive(it->second, recusion_level, result);
				if(res != bencode_ok)
				{
					return res;
				}
			}
			result += 'e';
		}
		break;
    default: assert(false); return bencode_err_unsupported_type;
	}

	return bencode_ok;
}

bdecode_result bdecode(const std::string& info, entry& result)
{
	size_t recursion_level = 0;
	data_iterator_t data_begin = info.cbegin();
	return bdecode_recursive(data_begin, info.cend(), recursion_level, result);
}

bencode_result bencode(const entry& e, std::string& result)
{
	result.clear();
	size_t recursion_level = 0;
	return bencode_recursive(e, recursion_level, result);
}

} // namespace bencoding
} // namespace x3me
