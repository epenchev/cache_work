#include "precompiled.h"
#include "debug_filter.h"
#include "id_tag.h"
#include "xproxy_ctl/debug_cmd.h"

namespace dfilter
{

template <typename IPType>
IPType to_ip(const dcmd::ast::ip_mask& v)
{
    IPType res;

    static_assert(std::is_same<decltype(res.mask_), uint32_t>::value, "");
    if (v.mask_ == dcmd::ast::ip_mask::no_mask)
        res.mask_ = 0xFFFFFFFFU;
    else if ((v.mask_ > 0) && (v.mask_ <= 32))
        res.mask_ = 0xFFFFFFFFU << (32 - v.mask_);
    else
        throw std::logic_error("Invalid CIDR mask value");

    res.ip_ = (uint32_t(v.b0_) << 24) + (uint32_t(v.b1_) << 16) +
              (uint32_t(v.b2_) << 8) + v.b3_;

    return res;
}

////////////////////////////////////////////////////////////////////////////////

ast_convertor::result_type ast_convertor::
operator()(const dcmd::ast::group_expr& rhs)
{
    ast::group_expr res;
    res.first_ = boost::apply_visitor(*this, rhs.first_);
    res.rest_.reserve(rhs.rest_.size());
    for (const auto& op : rhs.rest_)
    {
        const char o =
            (op.op_ == "or ") ? '|' : ((op.op_ == "and ") ? '&' : ' ');
        if (o == ' ')
            throw std::logic_error("Invalid operation neither 'or' nor 'and'.");
        auto r = (*this)(op);
        res.rest_.push_back(ast::operation{std::move(r), o});
    }
    return result_type{res};
}

ast_convertor::result_type ast_convertor::
operator()(const dcmd::ast::operation& rhs)
{
    return boost::apply_visitor(*this, rhs.operand_);
}

ast_convertor::result_type ast_convertor::
operator()(const dcmd::ast::log_level& rhs)
{
    has_log_level_ = true;
    using namespace xlog;
    for (level_type i = 0; i < to_number(level::num_levels); ++i)
    {
        if (strcasecmp(rhs.val_.c_str(), level_str(to_level(i))) == 0)
        {
            return result_type{ast::log_level{i}};
        }
    }
    throw std::logic_error("Invalid log level.");
    return result_type{ast::log_level{to_number(level::off)}};
}

ast_convertor::result_type ast_convertor::
operator()(const dcmd::ast::src_ip& rhs)
{
    return result_type{to_ip<ast::src_ip>(rhs)};
}

ast_convertor::result_type ast_convertor::
operator()(const dcmd::ast::dst_ip& rhs)
{
    return result_type{to_ip<ast::dst_ip>(rhs)};
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const ast::ip_mask& rhs) noexcept
{
    os << ip_addr4_t(rhs.ip_) << '/' << __builtin_popcount(rhs.mask_);
    return os;
}

void printer::operator()(const ast::group_expr& rhs) const noexcept
{
    boost::apply_visitor(*this, rhs.first_);
    for (const auto& op : rhs.rest_)
        (*this)(op);
}

void printer::operator()(const ast::operation& rhs) const noexcept
{
    boost::apply_visitor(*this, rhs.operand_);
    std::cout << "op: " << rhs.op_ << "; ";
}

void printer::operator()(const ast::log_level& rhs) const noexcept
{
    std::cout << "lvl: " << xlog::level_str(xlog::to_level(rhs.val_)) << "; ";
}

void printer::operator()(const ast::src_ip& rhs) const noexcept
{
    std::cout << "src_ip: " << rhs << "; ";
}

void printer::operator()(const ast::dst_ip& rhs) const noexcept
{
    std::cout << "dst_ip: " << rhs << "; ";
}

////////////////////////////////////////////////////////////////////////////////

filter::result_type filter::operator()(const ast::group_expr& rhs) const
    noexcept
{
    bool res = boost::apply_visitor(*this, rhs.first_);
    for (const auto& op : rhs.rest_)
    {
        switch (op.op_)
        {
        case '&':
            res = res && (*this)(op);
            break;
        case '|':
            res = res || (*this)(op);
            break;
        default:
            abort();
            break;
        }
    }
    return res;
}

filter::result_type filter::operator()(const ast::operation& rhs) const noexcept
{
    return boost::apply_visitor(*this, rhs.operand_);
}

filter::result_type filter::operator()(const ast::log_level& rhs) const noexcept
{
    return (msg_lvl_ <= rhs.val_);
}

filter::result_type filter::operator()(const ast::src_ip& rhs) const noexcept
{
    return ((msg_tag_.user_ip_num() & rhs.mask_) == (rhs.ip_ & rhs.mask_));
}

filter::result_type filter::operator()(const ast::dst_ip& rhs) const noexcept
{
    return ((msg_tag_.server_ip_num() & rhs.mask_) == (rhs.ip_ & rhs.mask_));
}

} // namespace dfilter
