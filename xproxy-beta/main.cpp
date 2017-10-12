#include "precompiled.h"
#include "settings.h"
#include "xproxy.h"

#ifdef X3ME_APP_TEST
tcp_endpoint_t g_server_ep;
#endif

int main(int argc, char** argv)
{
    using namespace x3me;

    int res = EXIT_SUCCESS;

    namespace po = boost::program_options;

    po::options_description desc("Options");
    // clang-format off
    desc.add_options()
        ("help,H", "This help message")
        ("config,C", po::value<std::string>()->default_value("./xproxy.cfg"),
            "Path to the config file")
        ("info,I", "Info about the binary")
        ("reset,R", "Resets the cache volumes 'erasing' all data")
        ("quick-exit,Q", "The application exits as soon as it's safe from "
            "the cache point of view without graceful shutdown")
#ifndef X3ME_APP_TEST
        ;
#else
        ("sip", po::value<std::string>()->required(), 
            "The IP of the server to connect to")
        ("sport", po::value<uint16_t>()->required(),
            "The port of the server to connect to");
#endif // X3ME_APP_TEST
    // clang-format on

    try
    {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

#ifdef X3ME_APP_TEST
        // Must be provided
        g_server_ep.address(
            ip_addr_t::from_string(vm["sip"].as<std::string>()));
        g_server_ep.port(vm["sport"].as<uint16_t>());
#endif // X3ME_APP_TEST

        if (vm.count("help"))
        {
            std::cout << desc << '\n';
        }
        else if (vm.count("info"))
        {
            std::cout << "X3ME HTTP Proxy.\n"
                         "Build datetime: "
                      << utils::get_build_datetime() << '\n'
                      << "GitHash: " << utils::get_git_hash() << '\n';
        }
        else if (vm.count("config"))
        {
            if (!sys_utils::dump_stacktrace_on_fatal_signal(STDERR_FILENO,
                                                            "XProxy crashed."))
            {
                std::cerr << "Unable to setup the stacktrace dumper. The "
                             "functionality needs super-user privileges\n";
                return EXIT_FAILURE;
            }

            settings sts;

            if (!sts.load(vm["config"].as<std::string>()))
                return EXIT_FAILURE;

            if (!init_logging(sts))
                return EXIT_FAILURE;

            XLOG_INFO(main_tag, "Start with settings: {}", sts);

            xproxy x(sts);
            res = x.run(vm.count("reset"), vm.count("quick-exit"))
                      ? EXIT_SUCCESS
                      : EXIT_FAILURE;
        }
        else
        {
            std::cerr << "No options provided.\n" << desc << '\n';
            res = EXIT_FAILURE;
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Unable to parse the provided options. " << ex.what()
                  << '\n' << desc << '\n';
        res = EXIT_FAILURE;
    }

    return res;
}
