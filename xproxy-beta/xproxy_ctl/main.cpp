#include "precompiled.h"
#include "xproxy_ctl.h"
#include "settings.h"

int main(int argc, char** argv)
{
    using namespace x3me;

    int res = EXIT_SUCCESS;

    if (!sys_utils::dump_stacktrace_on_fatal_signal(STDERR_FILENO,
                                                    "XProxy_ctl crashed."))
    {
        std::cerr << "Unable to setup the stacktrace dumper\n";
        return EXIT_FAILURE;
    }

    namespace po = boost::program_options;

    po::options_description desc("Options");
    desc.add_options()("help,H", "This help message")(
        "debug,D", "Shows debug info about execution")(
        "info,I", "Shows info about the binary")(
        "config,C", po::value<std::string>()->default_value("./xproxy.cfg"),
        "Path to the config file")(
        "exec,E", po::value<std::vector<std::string>>(),
        "Command to be executed. Option name could be omitted.");
    po::positional_options_description pdesc;
    pdesc.add("exec", -1);

    try
    {
        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .positional(pdesc)
                      .run(),
                  vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << '\n';
        }
        else if (vm.count("info"))
        {
            std::cout << "X3ME XProxy Controller.\n"
                         "Build datetime: "
                      << utils::get_build_datetime() << '\n'
                      << "GitHash: " << utils::get_git_hash() << '\n';
        }
        else if (vm.count("exec"))
        {
            settings sts;
            if (!sts.load(vm["config"].as<std::string>()))
                return EXIT_FAILURE;

            debug_log::enable(vm.count("debug"));
            const auto cmd_parts = vm["exec"].as<std::vector<std::string>>();
            const auto cmd = std::accumulate(
                cmd_parts.cbegin(), cmd_parts.cend(), std::string(),
                [](const auto& lhs, const auto& rhs)
                {
                    if (!lhs.empty())
                        return lhs + ' ' + rhs;
                    return rhs;
                });
            xproxy_ctl x(sts);
            res = x.exec(cmd) ? EXIT_SUCCESS : EXIT_FAILURE;
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
