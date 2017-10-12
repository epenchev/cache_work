#pragma once

namespace dcmd
{

std::ostream& operator<<(std::ostream& os, const ast::ip_mask& rhs) noexcept
{
    os << static_cast<uint16_t>(rhs.b0_) << '.'
       << static_cast<uint16_t>(rhs.b1_) << '.'
       << static_cast<uint16_t>(rhs.b2_) << '.'
       << static_cast<uint16_t>(rhs.b3_) << '/'
       << static_cast<uint16_t>(rhs.mask_);
    return os;
}

struct printer
{
    using result_type = void;

    void operator()(const ast::group_expr& rhs) const noexcept
    {
        boost::apply_visitor(*this, rhs.first_);
        for (const auto& op : rhs.rest_)
            (*this)(op);
    }
    void operator()(const ast::operation& rhs) const noexcept
    {
        boost::apply_visitor(*this, rhs.operand_);
        std::cout << "op: " << rhs.op_ << "; ";
    }
    void operator()(const ast::log_level& rhs) const noexcept
    {
        std::cout << "lvl: " << rhs.val_ << "; ";
    }
    void operator()(const ast::src_ip& rhs) const noexcept
    {
        std::cout << "src_ip: " << rhs << "; ";
    }
    void operator()(const ast::dst_ip& rhs) const noexcept
    {
        std::cout << "dst_ip: " << rhs << "; ";
    }
};

} // namespace dcmd
