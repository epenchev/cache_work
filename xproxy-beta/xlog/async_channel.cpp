#include "precompiled.h"
#include "async_channel.h"
#include "async_channel_impl.h"
#include "log_target.h"

namespace xlog
{

async_channel::async_channel(impl_t&& impl) noexcept : impl_(std::move(impl))
{
}
async_channel::~async_channel() noexcept
{
}

async_channel::async_channel(async_channel&& rhs) noexcept
    : impl_(std::move(rhs.impl_))
{
}

async_channel& async_channel::operator=(async_channel&& rhs) noexcept
{
    impl_ = std::move(rhs.impl_);
    return *this;
}

bool async_channel::add_log_target(const target_id& tid,
                                   log_target&& t) noexcept
{
    return impl_->add_log_target(tid, std::move(t.impl_));
}

bool async_channel::add_explicit_log_target(const target_id& tid,
                                            log_target&& t) noexcept
{
    return impl_->add_explicit_log_target(tid, std::move(t.impl_));
}

async_channel create_async_channel(const string_view_t& name, uint32_t hard_lim,
                                   uint32_t soft_lim) noexcept
{
    return async_channel(
        std::make_unique<detail::async_channel_impl>(name, hard_lim, soft_lim));
}

} // namespace xlog
