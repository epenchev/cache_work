#pragma once

namespace xlog
{
class log_target;
class target_id;
namespace detail
{
class async_channel_impl;
} // namespace detail
////////////////////////////////////////////////////////////////////////////////

class async_channel
{
    using impl_t = std::unique_ptr<detail::async_channel_impl>;

    template <typename Tag>
    friend class logger;
    impl_t impl_;

public:
    explicit async_channel(impl_t&& impl) noexcept;
    ~async_channel() noexcept;

    async_channel(async_channel&& rhs) noexcept;
    async_channel& operator=(async_channel&& rhs) noexcept;

    async_channel(const async_channel&) = delete;
    async_channel& operator=(const async_channel&) = delete;

    /// Returns true if the target is successfully added and false otherwise.
    /// It fails when the max number of allowed targets has been reached.
    bool add_log_target(const target_id& tid, log_target&& t) noexcept;

    /// The explicit log target gets messages only when they are explicitly
    /// sent to it.
    bool add_explicit_log_target(const target_id& tid, log_target&& t) noexcept;

#ifdef X3ME_TEST
    auto impl() noexcept { return impl_.get(); }
#endif
};

/// The function creates async_channel with the given hard limit and soft limit
/// values. The hard limit is the max count of pending messages on this channel.
/// The next messages are going to be dropped until the value of soft limit is
/// reached. When the soft limit values is reached, an auto generated error
/// message with the number of skipped/dropped messages is logged.
/// The soft limit must be smaller than the hard limit.
/// The name set to this channel must be no longer than 15 characters.
async_channel create_async_channel(const string_view_t& name, uint32_t hard_lim,
                                   uint32_t soft_lim) noexcept;

} // namespace xlog
