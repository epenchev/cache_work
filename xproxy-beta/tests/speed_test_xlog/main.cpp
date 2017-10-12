#include "precompiled.h"

class id_tag
{
    using sess_id_t     = void*;
    sess_id_t sess_id_  = (sess_id_t)0xDEADC0DE;
    uint16_t trans_id_  = 42;
    uint16_t module_id_ = 1;
    uint32_t user_ip_   = 0xBEEF0123;
    uint32_t serv_ip_   = 0xCAFECAFE;
    uint16_t user_po_   = 51234;
    uint16_t serv_po_   = 80;

private:
    friend std::ostream& operator<<(std::ostream& os,
                                    const id_tag& rhs) noexcept
    {
        // Tag printng - {sid}{tid}{uip:upo}{sip}
        // Module id is intentionally not printed at the moment.
        // If needed there should be some mapping between the numeric id and
        // name.
        using addr_t = boost::asio::ip::address_v4;
        os << '{' << rhs.sess_id_ << "}{" << rhs.trans_id_ << "}{"
           << addr_t(rhs.user_ip_) << ':' << rhs.user_po_ << "}{"
           << addr_t(rhs.serv_ip_) << '}';
        return os;
    }
};

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
    // Expects number of threads to run
    if (argc < 2)
    {
        std::cerr << "Provide number of threads as a numeric argument\n";
        return 1;
    }
    const uint32_t num_threads = atoi(argv[1]);
    if ((num_threads < 1) || (num_threads > 32))
    {
        std::cerr << "Passed number of threads must be >= 1 and <= 32\n";
        return 1;
    }

    auto xlg = xlog::create_logger<id_tag>();

    { // Init the logger with one async_channel and one file target
        constexpr xlog::channel_id chan_id(1);
        constexpr xlog::target_id tgt_id(1);
        // We don't want to reach the limit and throw messages, thus put some
        // high number.
        constexpr uint32_t hard_lim_pending = 10 * 1024 * 1024;
        constexpr uint32_t soft_lim_pending = hard_lim_pending - 1024;
        constexpr bool truncate             = true;

        boost::system::error_code err;
        auto chan = xlog::create_async_channel("test_chan", hard_lim_pending,
                                               soft_lim_pending);
        chan.add_log_target(
            tgt_id, xlog::create_file_target("./speed_test.log", truncate,
                                             xlog::level::info, err));
        xlg->add_async_channel(chan_id, std::move(chan));
    }

    static id_tag tag;
    static constexpr auto log_str           = "this is test log message";
    static constexpr uint32_t cnt_all_lines = 1024 * 1024;
    static const uint32_t cnt_lines         = cnt_all_lines / num_threads;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    // Measure
    using namespace std::chrono;
    const auto beg = high_resolution_clock::now();
    for (uint32_t i = 0; i < num_threads; ++i)
    {
        threads.push_back(
            std::thread([&xlg]
                        {
                            for (uint32_t i = 0; i < cnt_lines; ++i)
                            {
                                xlg->write(xlog::level::info, tag,
                                           "Str: {:s}. Num: {:d}", log_str, i);
                            }
                        }));
    }
    for (auto& t : threads)
    {
        if (t.joinable())
            t.join();
    }
    const auto end = high_resolution_clock::now();

    std::cout << "Written lines: " << cnt_lines * num_threads << ". All time: "
              << duration_cast<milliseconds>(end - beg).count()
              << " milliseconds. Single line avg. time: "
              << duration_cast<nanoseconds>(end - beg).count() /
                     (cnt_lines * num_threads)
              << " nanoseconds" << std::endl;

    return 0;
}
