#pragma once

// TODO_ Split this header as global_config.h and types.h, or rename it to global_config.h
// NOTE_ Here will be placed header files that are needed of the other x3me libraries, for easy start new project

#if _MSC_VER
#define X3ME_MICROSOFT_COMPILER 1
#define THREAD_VAR __declspec(thread)
#elif __GNUC__
#define X3ME_GNU_COMPILER 1
#define THREAD_VAR __thread
#define GCC_VERSION (__GNUC__ * 100 + __GNUC_MINOR__)

#else
#error Unsupported compiler
#endif

#if _WIN32 || _WIN64
#define X3ME_WIN_PLATFORM 1
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif __linux__
#define X3ME_LINUX_PLATFORM 1
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#else
#error Unsupported platform
#endif

#include <assert.h>
#include <string.h>

#include <iostream>
#include <iterator>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <set>
#include <list>
#include <map>
#include <unordered_map>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <chrono>
// string_builder needs of these two headers
#include <ostream>
#include <streambuf>

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef secure_sprintf // macro is used to see the exact function when failure happens

#ifdef X3ME_MICROSOFT_COMPILER
#define __x3me_ssnprintf__ ::_snprintf
#else
#define __x3me_ssnprintf__ ::snprintf
#endif

#define secure_sprintf(buffer, buffer_size, format, ...)																\
		int _out_len_ = __x3me_ssnprintf__(buffer, buffer_size, format, __VA_ARGS__);									\
		if(_out_len_ <= 0 || _out_len_ >= static_cast<int>(buffer_size))												\
			std::cout << "SPRINTF WARNING! buffer_size=" << buffer_size << ". out_length=" << _out_len_					\
					  << ". function=" << __FUNCTION__ << ". file=" << __FILE__ << ". line=" << __LINE__ << std::endl
		//assert(_out_len_ > 0 && _out_len_ < static_cast<int>(buffer_size))

#endif

#if !defined(_countof)
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define X3ME_ALLOC(size)					::operator new(size)
#define X3ME_FREE(ptr)						::operator delete(ptr)
#define X3ME_NEW(type)						new (std::nothrow) type		// usage - X3ME_NEW(some_type) or X3ME_NEW(some_type)(constructor_params)
#define X3ME_PLACEMENT_NEW(buffer, type)	new (buffer) type			// usage - X3ME_PLACEMENT_NEW(buffer, some_type) or X3ME_PLACEMENT_NEW(buffer, some_type)(constructor_params)
#define X3ME_DELETE(ptr)					delete ptr
#define X3ME_NEW_ARRAY(type, count)			new (std::nothrow) type [count]
#define X3ME_DELETE_ARRAY(ptr)				delete [] ptr
#define X3ME_ALLOCATOR(type)				std::allocator<type>
#define X3ME_DELETER(type)					x3me::types::deleter<type>
#define X3ME_ARRAY_DELETER(type)			x3me::types::array_deleter<type>

#define X3ME_MAKE_ALLOCATOR(type)			X3ME_ALLOCATOR(type)()			
#define X3ME_MAKE_DELETER(type)				X3ME_DELETER(type)()
#define X3ME_MAKE_ARRAY_DELETER(type)		X3ME_ARRAY_DELETER(type)()

// NOTE ## enable no va_args
#define X3ME_ALLOCATE_UNIQUE(type, ...)			x3me::types::unique_ptr_t<type>::type(X3ME_NEW(type)(##__VA_ARGS__))
#define X3ME_ALLOCATE_UNIQUE_ARRAY(type, count)	x3me::types::unique_array_ptr_t<type>::type(X3ME_NEW_ARRAY(type, count))
#define X3ME_ALLOCATE_SHARED(type, ...)			std::allocate_shared<type>(X3ME_MAKE_ALLOCATOR(type), ##__VA_ARGS__)

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace x3me
{
namespace types
{
	template<class T> struct deleter
	{
		void operator()(T* p) const
		{
			static_assert(sizeof(T) > 0, ""); // won't compile for incomplete type
			X3ME_DELETE(p);
		}
	};

	template<class T> struct deleter<T[]>
	{
		void operator()(T* p) const
		{
			static_assert(sizeof(T) > 0, ""); // won't compile for incomplete type
			X3ME_DELETE_ARRAY(p);
		}
	};

	template<class T> struct array_deleter
	{
		void operator()(T* p) const
		{
			static_assert(sizeof(T) > 0, ""); // won't compile for incomplete type
			X3ME_DELETE_ARRAY(p);
		}
	};

	template<typename T>
	struct unique_ptr_t
	{
		typedef std::unique_ptr<T, X3ME_DELETER(T)> type;
	};
	template<typename T>
	struct unique_array_ptr_t
	{
		typedef std::unique_ptr<T[], X3ME_ARRAY_DELETER(T)> type;
	};
	template<typename T>
	struct vector_t
	{
		typedef std::vector<T, X3ME_ALLOCATOR(T)> type;
	};
	template<typename T>
	struct deque_t
	{
		typedef std::deque<T, X3ME_ALLOCATOR(T)> type;
	};
	template<typename T>
	struct queue_t
	{
		typedef std::queue<T, typename deque_t<T>::type> type;
	};
	template<typename T>
	struct list_t
	{
		typedef std::list<T, X3ME_ALLOCATOR(T)> type;
	};
	template<typename T, typename Comparator = std::less<T>>
	struct set_t
	{
		typedef std::set<T, Comparator, X3ME_ALLOCATOR(T)> type;
	};
	template<typename T, typename Comparator = std::less<T>>
	struct multiset_t
	{
		typedef std::multiset<T, Comparator, X3ME_ALLOCATOR(T)> type;
	};
	template<typename Key, typename Value, typename Comparator = std::less<Key>>
	struct map_t
	{
		typedef std::pair<const Key, Value>											key_value_type;
		typedef std::map<Key, Value, Comparator, X3ME_ALLOCATOR(key_value_type)>	type;
	};
	template<typename Key, typename Value, typename Comparator = std::less<Key>>
	struct multimap_t
	{
		typedef std::pair<const Key, Value>												key_value_type;
		typedef std::multimap<Key, Value, Comparator, X3ME_ALLOCATOR(key_value_type)>	type;
	};
	template<typename Key, typename Value>
	struct unordered_map_t
	{
		typedef std::pair<const Key, Value> key_value_type;
		typedef std::unordered_map<Key, Value, std::hash<Key>, std::equal_to<Key>, X3ME_ALLOCATOR(key_value_type)> type;
	};
	template<typename Key, typename Value>
	struct unordered_multimap_t
	{
		typedef std::pair<const Key, Value> key_value_type;
		typedef std::unordered_multimap<Key, Value, std::hash<Key>, std::equal_to<Key>, X3ME_ALLOCATOR(key_value_type)> type;
	};

	typedef std::basic_string<char, std::char_traits<char>, X3ME_ALLOCATOR(char)> string_t;

	typedef vector_t<string_t>::type string_array_t;
} // namespace types

template<typename Cont>
auto inline begin(Cont& c) -> decltype(c.begin())
{
	return c.begin();
}
template<typename Cont>
auto inline begin(const Cont& c) -> decltype(c.begin())
{
	return c.begin();
}
template<typename Cont>
auto inline end(Cont& c) -> decltype(c.end())
{
	return c.end();
}
template<typename Cont>
auto inline end(const Cont& c) -> decltype(c.end())
{
	return c.end();
}
template<typename DataType, size_t ArrSize> 
inline DataType* begin(DataType (&arr)[ArrSize])
{
	return arr;
}
template<typename DataType, size_t ArrSize> 
inline DataType* end(DataType (&arr)[ArrSize])
{
	return (arr + ArrSize);
}

} // namespace x3me
