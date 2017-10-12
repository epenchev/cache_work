#pragma once

#include "xlog/xlog_common.h"

// 1 - Final type, 2 - Setting type, 3 - Setting section, 4 - Setting name
#define XPROXY_SETTINGS(MACRO)                                                 \
    MACRO(ip_addr4_t, std::string, main, bind_ip)                              \
    MACRO(double, double, main, scale_factor)                                  \
    MACRO(uint32_t, uint32_t, main, max_count_fds)                             \
    MACRO(uint16_t, uint16_t, main, bind_port)                                 \
    MACRO(uint16_t, uint16_t, main, kmod_def_window)                           \
    MACRO(uint16_t, uint16_t, main, dscp_hit)                                  \
    MACRO(uint16_t, uint16_t, main, dscp_miss)                                 \
    MACRO(std::string, std::string, cache, storage_cfg)                        \
    MACRO(uint16_t, uint16_t, cache, volume_threads)                           \
    MACRO(uint16_t, uint16_t, cache, min_avg_object_size_KB)                   \
    MACRO(std::string, std::string, plugins, cache_url_cfg)                    \
    MACRO(std::string, std::string, plugins, host_stats_cfg)                   \
    MACRO(ip_addr4_t, std::string, mgmt, bind_ip)                              \
    MACRO(uint16_t, uint16_t, mgmt, bind_port)                                 \
    MACRO(name_str, std::string, priv, user)                                   \
    MACRO(dir_path_str, std::string, priv, work_dir)                           \
    MACRO(dir_path_str, std::string, log, logs_dir)                            \
    MACRO(xlog::level, std::string, log, main_log_level)                       \
    MACRO(xlog::level, std::string, log, sys_log_level)                        \
    MACRO(xlog::level, std::string, log, debug_log_level)                      \
    MACRO(uint32_t, uint32_t, log, main_log_rotate_MB)                         \
    MACRO(uint32_t, uint32_t, log, squid_log_slide_MB)                         \
    MACRO(uint16_t, uint16_t, log, main_log_rotate_count)                      \
    MACRO(uint16_t, uint16_t, log, max_pending_records)

class settings
{
public:
    struct dir_path_str : std::string
    {
    };
    struct name_str : std::string
    {
    };

private:
#define MEMBER_DATA_IT(type, u0, section, name)                                \
    type section##_##name##_ = type();
    XPROXY_SETTINGS(MEMBER_DATA_IT)
#undef MEMBER_DATA_IT

public:
    settings();
    ~settings();

    settings(const settings&) = delete;
    settings& operator=(const settings&) = delete;
    settings(settings&&) = delete;
    settings& operator=(settings&&) = delete;

    bool load(const std::string& file_path);

#define GET_FUNC_IT(type, u0, section, name)                                   \
    const type& section##_##name() const { return section##_##name##_; }
    XPROXY_SETTINGS(GET_FUNC_IT)
#undef GET_FUNC_IT
};

std::ostream& operator<<(std::ostream& os, const settings& rhs);
