#pragma once

#include "xlog_common.h"

namespace xlog
{
namespace detail
{

class log_msg_tag
{
    const time_t timestamp_ = 0;
    const char* data_       = nullptr;
    const uint32_t size_    = 0;
    const target_id tid_    = invalid_target_id;
    const level_type lvl_   = to_number(level::off);

public:
    log_msg_tag(void* buff, uint32_t bufsize, time_t timestamp,
                const char* data, uint32_t size, target_id tid,
                level lvl) noexcept : timestamp_(timestamp),
                                      size_(size),
                                      tid_(tid),
                                      lvl_(to_number(lvl))
    {
        assert(bufsize >= size);
        auto d = static_cast<char*>(buff);
        memcpy(d, data, size);
        data_ = d;
    }
    ~log_msg_tag() noexcept = default;

    log_msg_tag() noexcept = delete;
    log_msg_tag(log_msg_tag&&) noexcept = delete;
    log_msg_tag& operator=(log_msg_tag&&) noexcept = delete;
    log_msg_tag(const log_msg_tag&) = delete;
    log_msg_tag& operator=(const log_msg_tag&) = delete;

    time_t timestamp() const noexcept { return timestamp_; }
    const char* data() const noexcept { return data_; }
    uint32_t size() const noexcept { return size_; }
    target_id get_target_id() const noexcept { return tid_; }
    level_type get_level() const noexcept { return lvl_; }
};

} // namespace detail
} // namespace xlog
