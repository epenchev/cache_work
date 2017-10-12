#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <boost/container/flat_map.hpp>

#include "json_rpc_common.h"

namespace x3me
{
namespace net
{
namespace json_rpc
{
namespace detail
{

class vc // value converter
{
    const value_t& v_;
    uint32_t pos_;

public:
    vc(const value_t& v, uint32_t pos) noexcept : v_(v), pos_(pos) {}

#define DEFINE_OPERATOR(data_type, rjs_type)                                   \
    operator data_type() const                                                 \
    {                                                                          \
        if (!v_.Is##rjs_type())                                                \
        {                                                                      \
            throw error(error_code::invalid_params,                            \
                        "Wrong argument type. Expects " #data_type             \
                        " type on position " +                                 \
                            std::to_string(pos_));                             \
        }                                                                      \
        return v_.Get##rjs_type();                                             \
    }

    DEFINE_OPERATOR(bool, Bool)
    DEFINE_OPERATOR(int32_t, Int)
    DEFINE_OPERATOR(uint32_t, Uint)
    DEFINE_OPERATOR(int64_t, Int64)
    DEFINE_OPERATOR(uint64_t, Uint64)
    DEFINE_OPERATOR(double, Double)

#undef DEFINE_OPERATOR

    operator string_ref_t() const
    {
        if (!v_.IsString())
        {
            throw error(
                error_code::invalid_params,
                "Wrong argument type. Expects string type on position " +
                    std::to_string(pos_));
        }
        return string_ref_t(v_.GetString(), v_.GetStringLength());
    }
};

template <typename Func, size_t CntArgs>
class executor
{
    template <size_t... Idx>
    static void run_impl(Func& func, json_rpc_res& out, const value_t& in,
                         std::index_sequence<Idx...>)
    {
        func(out, vc(in[Idx], Idx)...);
    }

public:
    static void run(Func& func, json_rpc_res& out, const value_t& in)
    {
        if (in.Size() != CntArgs)
        {
            throw error(error_code::invalid_params,
                        "Wrong arguments count. Expects " +
                            std::to_string(CntArgs) + " argument(s)");
        }
        run_impl(func, out, in, std::make_index_sequence<CntArgs>());
    }
};

////////////////////////////////////////////////////////////////////////////////

struct callback_base
{
    virtual ~callback_base() noexcept {}
    virtual void exec(json_rpc_res& out, const value_t& in) = 0;
};

template <typename... Args>
struct callback final : public callback_base
{
    using fn_t = std::function<void(json_rpc_res&, const Args&...)>;

    fn_t fn_;

public:
    explicit callback(const fn_t& fn) noexcept : fn_(fn) {}
    ~callback() noexcept final {}

    void exec(json_rpc_res& out, const value_t& in) final
    {
        executor<fn_t, sizeof...(Args)>::run(fn_, out, in);
    }
};

////////////////////////////////////////////////////////////////////////////////

class handler
{
    using cb_ptr_t = std::unique_ptr<callback_base>;
    using cont_t   = boost::container::flat_map<std::string, cb_ptr_t>;

    cont_t callbacks_;

public:
    handler() noexcept;
    ~handler() noexcept;

    handler(handler&&) noexcept;
    handler& operator=(handler&&) noexcept;

    handler(const handler&) = delete;
    handler& operator=(const handler&) = delete;

    template <typename... Args>
    bool add_callback(
        const char* name,
        const std::function<void(json_rpc_res&, const Args&...)>& cb) noexcept
    {
        cb_ptr_t p = std::make_unique<callback<Args...>>(cb);
        auto res   = callbacks_.emplace(name, std::move(p));
        return res.second;
    }

    void process(std::vector<uint8_t>& in,
                 const http_conn_ptr_t& conn) noexcept;
};

} // namespace detail
} // namespace json_rpc
} // namespace net
} // namesapce x3me
