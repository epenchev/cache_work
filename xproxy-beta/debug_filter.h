#pragma once

#include "xlog/xlog_common.h"

class id_tag;

namespace dcmd
{
namespace ast
{

struct group_expr;
struct operation;
struct log_level;
struct src_ip;
struct dst_ip;

} // namespace ast
} // namespace dcmd
////////////////////////////////////////////////////////////////////////////////

namespace dfilter
{
namespace ast
{

namespace x3 = boost::spirit::x3;

struct log_level
{
    xlog::level_type val_;
};

struct ip_mask
{
    uint32_t ip_;
    uint32_t mask_;
};

struct src_ip : ip_mask
{
};

struct dst_ip : ip_mask
{
};

struct group_expr;

struct filt_expr
    : x3::variant<log_level, src_ip, dst_ip, x3::forward_ast<group_expr>>
{
    using base_type::base_type;
    using base_type::operator=;
};

struct operation
{
    filt_expr operand_;
    char op_;
};

struct group_expr
{
    filt_expr first_;
    std::vector<operation> rest_;
};

} // namespace ast
////////////////////////////////////////////////////////////////////////////////
// We need to convert the debug command AST to a more effective AST variant

struct ast_convertor
{
    using result_type = ast::filt_expr;

    bool has_log_level_ = false;

    result_type operator()(const dcmd::ast::group_expr& rhs);
    result_type operator()(const dcmd::ast::operation& rhs);
    result_type operator()(const dcmd::ast::log_level& rhs);
    result_type operator()(const dcmd::ast::src_ip& rhs);
    result_type operator()(const dcmd::ast::dst_ip& rhs);
};

////////////////////////////////////////////////////////////////////////////////

struct printer
{
    using result_type = void;

    void operator()(const ast::group_expr& rhs) const noexcept;
    void operator()(const ast::operation& rhs) const noexcept;
    void operator()(const ast::log_level& rhs) const noexcept;
    void operator()(const ast::src_ip& rhs) const noexcept;
    void operator()(const ast::dst_ip& rhs) const noexcept;
};

////////////////////////////////////////////////////////////////////////////////
// The actual filter functionality

struct filter
{
    using result_type = bool;

    const id_tag& msg_tag_;
    xlog::level_type msg_lvl_;

    filter(const id_tag& tag, xlog::level lvl) noexcept
        : msg_tag_(tag),
          msg_lvl_(xlog::to_number(lvl))
    {
    }

    result_type operator()(const ast::group_expr& rhs) const noexcept;
    result_type operator()(const ast::operation& rhs) const noexcept;
    result_type operator()(const ast::log_level& rhs) const noexcept;
    result_type operator()(const ast::src_ip& rhs) const noexcept;
    result_type operator()(const ast::dst_ip& rhs) const noexcept;
};

} // namespace dfilter
