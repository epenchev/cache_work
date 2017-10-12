#pragma once

#include "json_rpc_common.h"

namespace x3me
{
namespace json
{

struct out_value
{
	value_t&			value;
	value_allocator_t&	allocator;

	out_value(value_t& v, value_allocator_t& a) : value(v), allocator(a) {}
};

namespace rpc
{

namespace detail
{
struct empty {};
}

struct callback_base
{
	virtual ~callback_base() {}
	virtual void exec(const value_t& in, out_value& out) = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TODO_ Do it with variadic templates

template<typename Func, size_t NumParams> struct executor;
template<typename CTList> struct  elements_counter;
template<typename T> struct callback_type;
template<typename T1, typename T2> struct callback_type2;
template<typename T1, typename T2, typename T3> struct callback_type3;

template<typename H, typename T>
struct callback_typelist : callback_base
{
	typedef H											head_t;
	typedef T											tail_t;
	typedef callback_typelist<H, T>						my_type_t;
	typedef typename callback_type<my_type_t>::func_t	func_t;

	enum { index = elements_counter<my_type_t>::value };

	func_t callback;

	explicit callback_typelist(func_t& func) : callback(std::move(func)) {}
	virtual ~callback_typelist() {}

	virtual void exec(const value_t& in, out_value& out)
	{
		executor<func_t, index>::run(callback, in, out);
	}
};

struct error_info
{
	error_code				code;
	x3me::types::string_t	msg;
	error_info(error_code c, const char* m) : code(c), msg(m) {}
	error_info(error_code c, const x3me::types::string_t& m) : code(c), msg(m) {}
};

#define CHECK_ARGS_COUNT(count)	if(in.Size() != count) throw error_info(INVALID_PARAMS, "method expects "#count" argument(s)")
#define DEFINE_OPERATOR(data_type, method)														\
	operator data_type () const																	\
	{																							\
		if(!v.Is##method())																		\
			throw error_info(INVALID_PARAMS, "wrong argument type. expects "#data_type" type");	\
		return v.Get##method();																	\
	}

struct vc // value_converter
{
	const value_t& v;
	explicit vc(const value_t& v_) : v(v_) {}
	DEFINE_OPERATOR(bool, Bool)
	DEFINE_OPERATOR(int, Int)
	DEFINE_OPERATOR(unsigned int, Uint)
	DEFINE_OPERATOR(long long, Int64)
	DEFINE_OPERATOR(unsigned long long, Uint64)
	DEFINE_OPERATOR(double, Double)
	operator x3me::types::string_t () const
	{																							
		if(!v.IsString())
			throw error_info(INVALID_PARAMS, "wrong argument type. expects string type");
		return x3me::types::string_t(v.GetString(), v.GetStringLength());																	
	}
	operator const value_t& () const { return v; }
};

template<typename Func> struct executor<Func, 0>
{
	static void run(Func& func, const value_t&/* in*/, out_value& out)
	{
		func(out);
	}
};
template<typename Func> struct executor<Func, 1>
{
	static void run(Func& func, const value_t& in, out_value& out)
	{
		CHECK_ARGS_COUNT(1);
		func(vc(in[0U]), out);
	}
};
template<typename Func> struct executor<Func, 2>
{
	static void run(Func& func, const value_t& in, out_value& out)
	{
		CHECK_ARGS_COUNT(2);
		func(vc(in[0U]), vc(in[1U]), out);
	}
};
template<typename Func> struct executor<Func, 3>
{
	static void run(Func& func, const value_t& in, out_value& out)
	{
		CHECK_ARGS_COUNT(3);
		func(vc(in[0U]), vc(in[1U]), vc(in[2U]), out);
	}
};

#undef CHECK_ARGS_COUNT
#undef DEFINE_OPERATOR

template<> struct elements_counter<callback_typelist<detail::empty, detail::empty>> { enum { value = 0 }; };
template<typename T> struct elements_counter<callback_typelist<T, detail::empty>> { enum { value = 1 }; };
template<typename T1, typename T2> struct elements_counter<callback_typelist<T1, T2>> { enum { value = 1 + elements_counter<T2>::value }; };

template<typename T1, typename T2, typename T3>
struct callback_type3<T1, T2, callback_typelist<T3, detail::empty>>
{
	typedef std::function<void (const T1&, const T2&, const T3&, out_value&)> func_t;
};
template<typename T1, typename T2, typename T3>
struct callback_type2<T1, callback_typelist<T2, T3>>
{
	typedef typename callback_type3<T1, T2, callback_typelist<typename T3::head_t, typename T3::tail_t>>::func_t func_t;
};
template<typename T1, typename T2>
struct callback_type2<T1, callback_typelist<T2, detail::empty>>
{
	typedef std::function<void (const T1&, const T2&, out_value&)> func_t;
};
template<typename T1, typename T2>
struct callback_type<callback_typelist<T1, T2>>
{
	typedef typename callback_type2<T1, callback_typelist<typename T2::head_t, typename T2::tail_t>>::func_t func_t;
};
template<typename T>
struct callback_type<callback_typelist<T, detail::empty>>
{
	typedef std::function<void (const T&, out_value&)> func_t;
};
template<>
struct callback_type<callback_typelist<detail::empty, detail::empty>>
{
	typedef std::function<void (out_value&)> func_t;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class handler
{
public:
	~handler();

	void add_callback(const char* name, std::function<void (out_value&)>&& callback);
	template<typename T>
	void add_callback(const char* name, std::function<void (const T&, out_value&)>&& callback);
	template<typename T1, typename T2>
	void add_callback(const char* name, std::function<void (const T1&, const T2&, out_value&)>&& callback);
	template<typename T1, typename T2, typename T3>
	void add_callback(const char* name, std::function<void (const T1&, const T2&, const T3&, out_value&)>&& callback);

	bool process(x3me::types::string_t& received_data, x3me::types::string_t& out_data, x3me::types::string_t* out_err_ptr);

private:
	void process(value_t& in_json, value_allocator_t& json_allocator,value_t& out_json);

private:
	//typedef x3me::types::unique_ptr_t<callback_base>::type					callback_ptr_t; // libcstd++ with gcc 4.5.2 doesn't support moving of std::pair, and it can't have map with unique_ptr
	typedef callback_base*														callback_ptr_t;
	typedef x3me::types::map_t<x3me::types::string_t, callback_ptr_t>::type		callbacks_t;
	
	callbacks_t m_callbacks;
};

inline void handler::add_callback(const char* name, std::function<void (out_value&)>&& callback)
{
	auto p = X3ME_NEW((callback_typelist<detail::empty, detail::empty>))(callback);
	m_callbacks.insert(callbacks_t::value_type(x3me::types::string_t(name), callback_ptr_t(p)));
}

template<typename T>
void handler::add_callback(const char* name, std::function<void (const T&, out_value&)>&& callback)
{
	auto p = X3ME_NEW((callback_typelist<T, detail::empty>))(callback);
	m_callbacks.insert(callbacks_t::value_type(x3me::types::string_t(name), callback_ptr_t(p)));
}

template<typename T1, typename T2>
void handler::add_callback(const char* name, std::function<void (const T1&, const T2&, out_value&)>&& callback)
{
	auto p = X3ME_NEW((callback_typelist<T1, callback_typelist<T2, detail::empty>>))(callback);
	m_callbacks.insert(callbacks_t::value_type(x3me::types::string_t(name), callback_ptr_t(p)));
}

template<typename T1, typename T2, typename T3>
void handler::add_callback(const char* name, std::function<void (const T1&, const T2&, const T3&, out_value&)>&& callback)
{
	auto p = X3ME_NEW((callback_typelist<T1, callback_typelist<T2, callback_typelist<T3, detail::empty>>>))(callback);
	m_callbacks.insert(callbacks_t::value_type(x3me::types::string_t(name), callback_ptr_t(p)));
}

} // namespace rpc
} // namespace json
} // namesapce x3me
