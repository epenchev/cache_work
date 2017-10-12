#pragma once

#include "mpl.h"

namespace x3me
{
namespace bencoding
{

class entry
{
public:
	typedef std::string							dictionary_key_t;
	typedef int64_t								integer_t;
	typedef std::string							string_t;
	typedef std::vector<entry>					list_t;
	typedef std::map<dictionary_key_t, entry>	dictionary_t;
		
	enum entry_type
	{
		type_undefined,
		type_integer,
		type_string,
		type_list,
		type_dictionary,s			
	};

private:
	typedef x3me::mpl::typelist<integer_t, string_t, list_t, dictionary_t>::max_size_type_t max_size_type_t;
	typedef std::aligned_storage<sizeof(max_size_type_t), std::alignment_of<max_size_type_t>::value>::type storage_t;
	entry_type	m_type;
	storage_t	m_data;

public:
	entry();
	entry(const entry& e);
	entry(entry&& e);
	explicit entry(const integer_t& integer_data);
	explicit entry(const string_t& string_data);
	explicit entry(integer_t&& integer_data);
	explicit entry(string_t&& string_data);
	explicit entry(list_t&& list_data);
	explicit entry(dictionary_t&& dictionary_data);
	entry& operator =(const entry& e);
	entry& operator =(entry&& e);
	~entry();

	entry_type get_type() const
	{
		return m_type;
	}

	const integer_t* get_integer() const
	{
		return (m_type == type_integer ? reinterpret_cast<const integer_t*>(&m_data) : nullptr);
	}
	const string_t* get_string() const
	{
		return (m_type == type_string ? reinterpret_cast<const string_t*>(&m_data) : nullptr);
	}
	const list_t* get_list() const
	{
		return (m_type == type_list ? reinterpret_cast<const list_t*>(&m_data) : nullptr);
	}
	const dictionary_t* get_dictionary() const
	{
		return (m_type == type_dictionary ? reinterpret_cast<const dictionary_t*>(&m_data) : nullptr);
	}

	void set_integer(integer_t&& i);
	void set_string(string_t&& s);
	void set_list(list_t&& l);
	void set_dictionary(dictionary_t&& d);

private:
	void destruct();

	void construct_copy_entry(const entry& e);
	void construct_move_entry(entry&& e);

	void copy_same_type_entry(const entry& e);
	void move_same_type_entry(entry&& e);

	template<int type_of_entry, typename T>
	void set_data(T&& data);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum bdecode_result
{
	bdecode_ok,

	bdecode_err_recursion_level,

	bdecode_err_not_terminated_integer,
	bdecode_err_broken_integer,

	bdecode_err_not_terminated_list,

	bdecode_err_dictionary_key_not_string,
	bdecode_err_not_terminated_dictionary,

	bdecode_err_string_length_delimiter_not_present,
	bdecode_err_broken_string_length,
	bdecode_err_negative_string_length,
	bdecode_err_shortened_string,
};
enum bencode_result
{
	bencode_ok,
	bencode_err_recursion_level,
    bencode_err_unsupported_type,
};

bdecode_result bdecode(const std::string& info, entry& result);
bencode_result bencode(const entry& e, std::string& result);

} // namespace bencoding
} // namespace x3me
