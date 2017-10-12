#pragma once

// 1 - Final type, 2 - Setting type, 3 - Setting section, 4 - Setting name
#define XPROXY_SETTINGS(MACRO)                                                 \
    MACRO(ip_addr4_t, std::string, mgmt, bind_ip)                              \
    MACRO(uint16_t, uint16_t, mgmt, bind_port)

class settings
{
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
