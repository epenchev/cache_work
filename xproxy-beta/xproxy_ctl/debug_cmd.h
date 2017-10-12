#pragma once

// You'll need to be familiar how the boost::spirit::x3 works in order to
// understand the below code.

namespace dcmd
{
namespace x3 = boost::spirit::x3;

namespace ast
{
// The members of the Abstract Syntax Tree

struct log_level
{
    std::string val_;
};

struct ip_mask
{
    static constexpr uint8_t no_mask = 0xFF;

    uint8_t b0_, b1_, b2_, b3_;
    uint8_t mask_ = no_mask;
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
    std::string op_;
    filt_expr operand_;
};

struct group_expr
{
    filt_expr first_;
    std::vector<operation> rest_;
};

} // namespace ast
} // namespace dcmd

// This defenition needs to be at global scope
BOOST_FUSION_ADAPT_STRUCT(dcmd::ast::log_level, val_)
BOOST_FUSION_ADAPT_STRUCT(dcmd::ast::src_ip, b0_, b1_, b2_, b3_, mask_)
BOOST_FUSION_ADAPT_STRUCT(dcmd::ast::dst_ip, b0_, b1_, b2_, b3_, mask_)
BOOST_FUSION_ADAPT_STRUCT(dcmd::ast::operation, op_, operand_)
BOOST_FUSION_ADAPT_STRUCT(dcmd::ast::group_expr, first_, rest_)

namespace dcmd
{
namespace grammar
{
// The accepted grammar

const x3::rule<class log_level_expr, ast::log_level>
    log_level_expr("log_level_expr");
const x3::rule<class src_ip_expr, ast::src_ip> src_ip_expr("src_ip_expr");
const x3::rule<class dst_ip_expr, ast::dst_ip> dst_ip_expr("dst_ip_expr");
const x3::rule<class filt_expr, ast::filt_expr> filt_expr("filt_expr");
const x3::rule<class group_expr, ast::filt_expr> group_expr("group_expr");
const x3::rule<class filt_exprs, ast::group_expr> filt_exprs("filt_exprs");

// Matches the xxx.xxx.xxx.xxx or xxx.xxx.xxx.xxx/mask
const auto ip_mask = x3::uint8 >> '.' >> x3::uint8 >> '.' >> x3::uint8 >> '.' >>
                     x3::uint8 >> -('/' >> x3::uint8);
// Use lexeme to stop when space is found. It's too greedy otherwise.
// Matches 'lvl <string>'. The string is checked as additional step.
const auto log_level_expr_def =
    x3::lit("lvl ") >> x3::lexeme[+x3::char_("a-z")];
// Matches 'src xxx.xxx.xxx.xxx' or 'src xxx.xxx.xxx.xxx/mask'
const auto src_ip_expr_def = x3::lit("src ") >> ip_mask;
// Matches 'dst xxx.xxx.xxx.xxx' or 'dst xxx.xxx.xxx.xxx/mask'
const auto dst_ip_expr_def = x3::lit("dst ") >> ip_mask;
// Matches one of the above three
const auto filt_expr_def = log_level_expr | src_ip_expr | dst_ip_expr;
// Matches single filter expression or group of filter expressions with or
// without braces.
const auto group_expr_def =
    filt_expr | '(' >> filt_expr >> ')' | '(' >> filt_exprs >> ')';
// Matches single group expression followed by 0 or more group expressions
// concatenated with 'and' or 'or'.
const auto filt_exprs_def = group_expr >> *((x3::string("and ") >> group_expr) |
                                            (x3::string("or ") >> group_expr));

// The warning comes from the boost::spirit code through the macro.
// In fact the GCC doesn't complain for it, clang does it.
// However, if I put clang in the pragma the GCC complains about the pragma :).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
BOOST_SPIRIT_DEFINE(log_level_expr, src_ip_expr, dst_ip_expr, filt_expr,
                    group_expr, filt_exprs);
#pragma GCC diagnostic pop

} // namespace grammar

const auto cmd_descr = grammar::filt_exprs;

} // namespace dcmd
