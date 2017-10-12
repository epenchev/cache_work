#pragma once

#include <cstdlib>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "short_alloc.h"
#include "utils.h"

// The functionality here is provided only for bencoding.
// That's why the interface of the entry may seem, kind of, incomplete.
// A different functionality, part of the libtorrent, is used for the
// bdecoding.

namespace x3me
{
namespace bencode
{

template <typename Arena>
class entry;
namespace detail
{
template <typename Arena>
void write_entry(std::ostream& os, const entry<Arena>& e, int recursion);
} // namespace detail
////////////////////////////////////////////////////////////////////////////////

enum
{
    alignment = 8
};

template <size_t N>
using stack_arena_t = x3me::mem::stack_arena<N, alignment>;
using heap_arena_t  = x3me::mem::heap_arena<alignment>;

////////////////////////////////////////////////////////////////////////////////

enum entry_type
{
    entry_int,
    entry_string,
    entry_list,
    entry_dict,
};

////////////////////////////////////////////////////////////////////////////////

template <typename Arena>
class entry
{
    template <typename T>
    using alloc_t = x3me::mem::short_alloc<T, Arena>;

public:
    using int_t = int64_t;
    using string_t =
        std::basic_string<char, std::char_traits<char>, alloc_t<char>>;
    using list_t     = std::vector<entry, alloc_t<entry>>;
    using dict_key_t = string_t;
    using dict_t     = std::map<dict_key_t, entry, std::less<dict_key_t>,
                            alloc_t<std::pair<const dict_key_t, entry>>>;

private:
    using string_alloc_t = alloc_t<char>;
    using list_alloc_t   = alloc_t<entry>;
    using dict_alloc_t   = alloc_t<std::pair<const dict_key_t, entry>>;

    enum
    {
        max_size_t = x3me::utils::max(sizeof(int_t), sizeof(string_t),
                                      sizeof(list_t), sizeof(dict_t)),
        max_align_t = x3me::utils::max(alignof(int_t), alignof(string_t),
                                       alignof(list_t), alignof(dict_t)),
    };
    using storage_t =
        typename std::aligned_storage<max_size_t, max_align_t>::type;

    storage_t data_;
    entry_type type_;

public:
    entry(Arena& a, entry_type t)
    {
        switch (t)
        {
        case entry_int:
            construct<int_t>(t);
            break;
        case entry_string:
            construct<string_t>(t, string_alloc_t(a));
            break;
        case entry_list:
            construct<list_t>(t, list_alloc_t(a));
            break;
        case entry_dict:
            // The GCC 4.8.2 libstdc++ doesn't provide constructor which takes
            // only allocator. It should provide such since C++11
            // construct<dict_t>(t, dict_alloc_t(a));
            construct<dict_t>(t, typename dict_t::key_compare(),
                              dict_alloc_t(a));
            break;
        default:
            assert(false);
            break;
        }
    }

    entry(Arena& a, int64_t n) { construct<int_t>(entry_int, n); }

    entry(Arena& a, const std::string& s)
    {
        construct<string_t>(entry_string, s, string_alloc_t(a));
    }
    entry(Arena& a, const char* s) // NULL terminated string
    {
        construct<string_t>(entry_string, s, string_alloc_t(a));
    }
    entry(Arena& a, const char* s, size_t l)
    {
        construct<string_t>(entry_string, s, l, string_alloc_t(a));
    }

    entry(const entry& rhs)
    {
        switch (rhs.type())
        {
        case entry_int:
            construct<int_t>(rhs.type(), rhs.as<int_t>());
            break;
        case entry_string:
            construct<string_t>(rhs.type(), rhs.as<string_t>());
            break;
        case entry_list:
            construct<list_t>(rhs.type(), rhs.as<list_t>());
            break;
        case entry_dict:
            construct<dict_t>(rhs.type(), rhs.as<dict_t>());
            break;
        default:
            assert(false);
            break;
        }
    }

    entry& operator=(const entry& rhs)
    {
        switch (rhs.type())
        {
        case entry_int:
            reconstruct<int_t>(rhs.type(), rhs.as<int_t>());
            break;
        case entry_string:
            reconstruct<string_t>(rhs.type(), rhs.as<string_t>());
            break;
        case entry_list:
            reconstruct<list_t>(rhs.type(), rhs.as<list_t>());
            break;
        case entry_dict:
            reconstruct<dict_t>(rhs.type(), rhs.as<dict_t>());
            break;
        default:
            assert(false);
            break;
        }
    }

    entry(entry&& rhs)
    {
        switch (rhs.type())
        {
        case entry_int:
            construct<int_t>(rhs.type(), std::move(rhs.as<int_t>()));
            break;
        case entry_string:
            construct<string_t>(rhs.type(), std::move(rhs.as<string_t>()));
            break;
        case entry_list:
            construct<list_t>(rhs.type(), std::move(rhs.as<list_t>()));
            break;
        case entry_dict:
            construct<dict_t>(rhs.type(), std::move(rhs.as<dict_t>()));
            break;
        default:
            assert(false);
            break;
        }
    }

    entry& operator=(entry&& rhs)
    {
        switch (rhs.type())
        {
        case entry_int:
            reconstruct<int_t>(rhs.type(), std::move(rhs.as<int_t>()));
            break;
        case entry_string:
            reconstruct<string_t>(rhs.type(), std::move(rhs.as<string_t>()));
            break;
        case entry_list:
            reconstruct<list_t>(rhs.type(), std::move(rhs.as<list_t>()));
            break;
        case entry_dict:
            reconstruct<dict_t>(rhs.type(), std::move(rhs.as<dict_t>()));
            break;
        default:
            assert(false);
            break;
        }
    }

    ~entry() noexcept
    {
        switch (type())
        {
        case entry_int:
            destruct<int_t>();
            break;
        case entry_string:
            destruct<string_t>();
            break;
        case entry_list:
            destruct<list_t>();
            break;
        case entry_dict:
            destruct<dict_t>();
            break;
        default:
            assert(false);
            break;
        }
    }

    entry_type type() const noexcept { return type_; }

    // Unsafe functions
    int_t& as_int()
    {
        assert(type_ == entry_int);
        return as<int_t>();
    }
    string_t& as_string()
    {
        assert(type_ == entry_string);
        return as<string_t>();
    }
    list_t& as_list()
    {
        assert(type_ == entry_list);
        return as<list_t>();
    }
    dict_t& as_dict()
    {
        assert(type_ == entry_dict);
        return as<dict_t>();
    }

    // Unsafe functions
    const int_t& as_int() const
    {
        assert(type_ == entry_int);
        return as<int_t>();
    }
    const string_t& as_string() const
    {
        assert(type_ == entry_string);
        return as<string_t>();
    }
    const list_t& as_list() const
    {
        assert(type_ == entry_list);
        return as<list_t>();
    }
    const dict_t& as_dict() const
    {
        assert(type_ == entry_dict);
        return as<dict_t>();
    }

private:
    template <typename T, typename... Args>
    void construct(entry_type type, Args&&... args)
    {
        new (&data_) T(std::forward<Args>(args)...);
        type_ = type;
    }

    template <typename T>
    void destruct()
    {
        as<T>().~T();
    }

    template <typename T, typename... Args>
    void reconstruct(entry_type type, Args&&... args)
    {
        destruct<T>();
        construct<T>(type, std::forward<Args>(args)...);
    }

    friend void detail::write_entry<Arena>(std::ostream&, const entry&, int);

    template <typename T>
    T& as()
    {
        return *reinterpret_cast<T*>(&data_);
    }
    template <typename T>
    const T& as() const
    {
        return *reinterpret_cast<const T*>(&data_);
    }
};

////////////////////////////////////////////////////////////////////////////////

namespace detail
{
template <typename Arena>
void write_entry(std::ostream& os, const entry<Arena>& e, int recursion)
{
    // I can't imagine a case when we'll need to recurse more than
    // 3-5 levels.
    // If a need for setting this parameter arises a different
    // streaming API is going to be provided
    enum
    {
        max_recursion = 20
    };
    if (++recursion > max_recursion)
    {
        std::abort();
        return;
    }
    using entry_t = entry<Arena>;
    switch (e.type())
    {
    case entry_int:
        os << 'i' << e.template as<entry_t::int_t>() << 'e';
        break;
    case entry_string:
    {
        const auto& s = e.template as<entry_t::string_t>();
        os << s.size() << ':' << s;
        break;
    }
    case entry_list:
    {
        const auto& l = e.template as<entry_t::list_t>();
        os << 'l';
        for (const auto& le : l)
            write_entry(os, le, recursion);
        os << 'e';
        break;
    }
    case entry_dict:
    {
        const auto& d = e.template as<entry_t::dict_t>();
        os << 'd';
        for (const auto& de : d)
        {
            os << de.first.size() << ':' << de.first;
            write_entry(os, de.second, recursion);
        }
        os << 'e';
        break;
    }
    default:
        assert(false);
        break;
    }
}
} // namespace detail

template <typename Arena>
std::ostream& operator<<(std::ostream& os, const entry<Arena>& rhs)
{
    write_entry(os, rhs, 0);
    return os;
}

} // namespace bencode
} // namespace x3me
