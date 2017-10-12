#include "precompiled.h"
#include "settings.h"

template <typename T>
bool load_to_final(const T& setting, T& out)
{
    out = setting;
    return true;
}

bool load_to_final(const std::string& addr, ip_addr4_t& out)
{
    err_code_t err;
    out = ip_addr4_t::from_string(addr, err);
    return !err;
}

////////////////////////////////////////////////////////////////////////////////

settings::settings()
{
}

settings::~settings()
{
}

bool settings::load(const std::string& file_path)
{
    namespace po = boost::program_options;

    bool result = true;

    try
    {
        std::ifstream ifs(file_path);
        if (!ifs)
        {
            std::cerr << "Unable to load the config file '" << file_path
                      << "'\n";
            return false;
        }

        po::options_description desc;

#define REGISTER_OPTION_IT(u0, type, section, name)                            \
    desc.add_options()(#section "." #name, po::value<type>());

        XPROXY_SETTINGS(REGISTER_OPTION_IT)

#undef REGISTER_OPTION_IT

        constexpr bool allow_unreg_opts = true;
        po::variables_map vm;
        po::store(po::parse_config_file(ifs, desc, allow_unreg_opts), vm);
        po::notify(vm);

#define GET_OPTION_VALUE_IT(u0, type, section, name)                           \
    if (vm.count(#section "." #name))                                          \
    {                                                                          \
        if (!load_to_final(vm[#section "." #name].as<type>(),                  \
                           section##_##name##_))                               \
        {                                                                      \
            std::cerr << "Invalid value '"                                     \
                      << vm[#section "." #name].as<type>()                     \
                      << "' for config file option '" << #section "." #name    \
                      << "'\n";                                                \
            result = false;                                                    \
        }                                                                      \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        std::cerr << "Missing config file option '" #section "." #name "'\n";  \
        result = false;                                                        \
    }

        XPROXY_SETTINGS(GET_OPTION_VALUE_IT)

#undef GET_OPTION_VALUE_IT
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Unable to load the config file '" << file_path << "'. "
                  << ex.what() << '\n';
        result = false;
    }

    return result;
}

std::ostream& operator<<(std::ostream& os, const settings& rhs)
{
#define DUMP_SETTING(u0, u1, section, name)                                    \
    os << "\n\t" #section "." #name " = " << rhs.section##_##name();
    XPROXY_SETTINGS(DUMP_SETTING)
#undef DUMP_SETTING
    return os;
}
