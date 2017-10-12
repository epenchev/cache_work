#include "precompiled.h"
#include "settings.h"
#include "xlog/xlog_common.h"

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

bool load_to_final(const std::string& lvl_str, xlog::level& out)
{
    using namespace xlog;
    // Not a very effective function but it's called only on application start.
    for (level_type i = 0; i < to_number(level::num_levels); ++i)
    {
        if (strcasecmp(lvl_str.c_str(), level_str(to_level(i))) == 0)
        {
            out = to_level(i);
            return true;
        }
    }
    std::cerr << "Invalid log level: " << lvl_str << '\n';
    return false;
}

bool load_to_final(const std::string& path, settings::dir_path_str& out)
{
    if (path.empty())
        return false;
    namespace fs = boost::filesystem;
    try
    {
        // Convert relative path to absolute to avoid later errors if the
        // working directory gets changed.
        const auto p = fs::absolute(path);
        fs::create_directories(p); // Ensure that the directory exists
        out.assign(p.native());
    }
    catch (const fs::filesystem_error& err)
    {
        std::cerr << "Missing permissions or invalid path '" << path << "'. "
                  << err.what() << '\n';
        return false;
    }
    return true;
}

bool load_to_final(const std::string& name, settings::name_str& out)
{
    if (name.empty())
        return false;
    out.assign(name);
    return true;
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

    // Checks which can't be done via the above method of overloaded
    // load_to_final
    if (log_max_pending_records_ < 50)
    {
        std::cerr
            << "The settings log.max_pending_records must not be less than "
            << 50 << '\n';
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
