#include "precompiled.h"
#include "host_stats.h"
#include "xutils/http_funcs.h"
#include "xutils/moveable_handler.h"
#include "protobuf/pb_stats.pb.h"
// Don't want to add building of .cc files to the main build file template
// (common.mk) just for this case. Thus the source is included here.
#include "protobuf/pb_stats.pb.cc"

// TODO Log

namespace plgns
{
namespace
{
struct stats_file
{
    int fd_;

    stats_file(const char* path, int flags) noexcept
        : fd_(::open(path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
    {
    }
    ~stats_file()
    {
        if (is_open())
            close(fd_);
    }

    stats_file(const stats_file&) = delete;
    stats_file& operator=(const stats_file&) = delete;
    stats_file(stats_file&&) = delete;
    stats_file& operator=(stats_file&&) = delete;

    bool is_open() const noexcept { return (fd_ != -1); }
};
} // namespace
////////////////////////////////////////////////////////////////////////////////

host_stats::host_stats(io_service_t& ios) noexcept : dump_tmr_(ios)
{
}

host_stats::~host_stats() noexcept
{
}

void host_stats::init(std::istream& cfg_data, const net_thread_exec& net_exec)
{
    load_settings(cfg_data);

    thread_stats_.resize(net_exec.cnt_threads_);
    net_thread_exec_ = net_exec.exec_;

    const auto ctm = current_time();
    set_curr_dump_fpath(ctm);
    curr_day_ = ctm.tm_mday;

    load_curr_stats();

    schedule_stats_dump();
}

bool host_stats::record_host(net_thread_id_t net_tid,
                             const string_view_t& url,
                             bytes64_t hit_bytes,
                             bytes64_t miss_bytes) noexcept
{
    const string_view_t host =
        xutils::truncate_host(xutils::get_host(url), settings_.domain_level_);
    if (X3ME_UNLIKELY(host.empty()))
        return false;
    XLOG_TRACE(
        plgn_tag,
        "Host_stats. Record host {} from URL {}. Hit_bytes {}. Miss_bytes {}",
        host, url, hit_bytes, miss_bytes);
    X3ME_ASSERT(net_tid < thread_stats_.size(), "Invalid net thread id");
    // Unfortunately the unordered containers doesn't provide heterogeneous
    // search, in the same way the ordered containers map/set provide since
    // C++14. Thus we need to make memory allocation and construct a string
    // object only of the search, and throw it away after that, in all of the
    // cases except the first insert :(.
    auto& cntrs = thread_stats_[net_tid][host.to_string()];
    cntrs.bytes_hit_ += hit_bytes;
    cntrs.bytes_miss_ += miss_bytes;
    cntrs.reqs_hit_ += (hit_bytes > 0);
    cntrs.reqs_miss_ += (hit_bytes == 0);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

void host_stats::load_settings(std::istream& cfg_data)
{
    try
    {
        namespace po = boost::program_options;
        po::options_description desc;
        desc.add_options()("domain_level", po::value<uint16_t>());
        desc.add_options()("dump_timeout", po::value<uint32_t>());
        desc.add_options()("stats_dir", po::value<std::string>());

        constexpr bool allow_unreg_opts = true;
        po::variables_map vm;
        po::store(po::parse_config_file(cfg_data, desc, allow_unreg_opts), vm);
        po::notify(vm);

        settings_.dump_dir_     = vm["stats_dir"].as<std::string>();
        settings_.domain_level_ = vm["domain_level"].as<uint16_t>();
        settings_.dump_timeout_ =
            std::chrono::seconds{vm["dump_timeout"].as<uint32_t>()};

        XLOG_INFO(plgn_tag, "Host_stats. Loaded settings. Domain_level {}. "
                            "Dump_timeout {} seconds. Stats_dir {}",
                  settings_.domain_level_, settings_.dump_timeout_.count(),
                  settings_.dump_dir_);
    }
    catch (const std::exception& ex)
    {
        throw std::invalid_argument("Unable to load config file data. " +
                                    std::string(ex.what()));
    }
}

void host_stats::load_curr_stats()
{
    stats_file file{curr_dump_fpath_.c_str(), O_RDONLY};
    if (!file.is_open())
    {
        if (errno == ENOENT) // It's OK if the file doesn't exist
            return;
        throw bsys::system_error(errno, bsys::get_system_category(),
                                 "Unable to open host stats file '" +
                                     curr_dump_fpath_ + "'");
    }

    pb_stats::Stats stats;
    using namespace google::protobuf;
    io::FileInputStream fis(file.fd_);
    io::CodedInputStream cis(&fis);
    cis.SetTotalBytesLimit(128_MB, 64_MB);
    if (!stats.ParseFromCodedStream(&cis))
    {
        throw std::runtime_error(
            "Unable to load host stats file '" + curr_dump_fpath_ +
            "'. It's probably corrupted. You can remove it, if you are sure");
    }

    // Copy the stats data to more appropriate data structure
    const auto& sd = stats.data();
    all_stats_.clear();
    all_stats_.reserve(sd.size());
    for (const auto& s : sd)
    {
        auto& e = all_stats_[s.host()];
        e.bytes_hit_ += s.bytes_hit();
        e.bytes_miss_ += s.bytes_miss();
        e.reqs_hit_ += s.reqs_hit();
        e.reqs_miss_ += s.reqs_miss();
    }

    XLOG_INFO(plgn_tag,
              "Host_stats. Loaded current stats from '{}' for {} hosts",
              curr_dump_fpath_, all_stats_.size());
}

////////////////////////////////////////////////////////////////////////////////

void host_stats::schedule_stats_dump() noexcept
{
    err_code_t err;
    dump_tmr_.expires_from_now(settings_.dump_timeout_, err);
    if (err)
    {
        std::cerr << "Host_stats dump timer setup failure. " << err.message()
                  << std::endl;
        ::abort();
    }
    dump_tmr_.async_wait([this](const err_code_t& err)
                         {
                             if (!err)
                             {
                                 do_async_stats_dump();
                             }
                             else if (err != asio_error::operation_aborted)
                             {
                                 std::cerr << "Host_stats dump timer failure. "
                                           << err.message() << std::endl;
                                 ::abort();
                             }
                         });
}

void host_stats::do_async_stats_dump() noexcept
{
    XLOG_DEBUG(plgn_tag, "Host_stats. Do_async_stats_dump");
    struct stats_collector
    {
        host_stats* inst_;
        io_service_t& ios_;
        host_stats_arr_t stats_;

        explicit stats_collector(host_stats* inst) noexcept
            : inst_(inst),
              ios_(inst->dump_tmr_.get_io_service()),
              stats_(inst->thread_stats_.size())
        {
        }

        ~stats_collector() noexcept
        {
            // The destructor is going to be called from some of the
            // network threads. Thus post it back to the worker thread,
            // to not stop the net thread while processing the stats.
            ios_.post(xutils::make_moveable_handler(
                [ inst = inst_, st = std::move(stats_) ]
                {
                    inst->do_stats_dump(st);
                    inst->schedule_stats_dump();
                }));
            X3ME_ASSERT(stats_.empty(), "Stats must have been moved");
        }
    };
    // Collect/reset every per thread statistic in the corresponding net thread,
    // avoiding the need for locking.
    auto sc = std::make_shared<stats_collector>(this);
    for (net_thread_id_t i = 0; i < thread_stats_.size(); ++i)
    {
        net_thread_exec_(i, [sc, i]
                         {
                             sc->stats_[i].swap(sc->inst_->thread_stats_[i]);
                         });
    }
}

void host_stats::do_stats_dump(const host_stats_arr_t& stats) noexcept
{
    // The all_stats are accessed only inside this function.
    // The protobuf v.3 offers map functionality which can be used here someday.
    host_stats_t all_stats;
    all_stats.swap(all_stats_);
    for (const auto& sts : stats)
    {
        for (const auto& st : sts)
        {
            auto& s = all_stats[st.first];
            s.bytes_hit_ += st.second.bytes_hit_;
            s.bytes_miss_ += st.second.bytes_miss_;
            s.reqs_hit_ += st.second.reqs_hit_;
            s.reqs_miss_ += st.second.reqs_miss_;
        }
    }
    // Copy the all_stats data to the protobuf container.
    pb_stats::Stats final_stats;
    {
        auto* fs = final_stats.mutable_data();
        fs->Reserve(all_stats.size());
        for (const auto& s : all_stats)
        {
            auto* e = fs->Add();
            e->set_host(s.first);
            e->set_bytes_hit(s.second.bytes_hit_);
            e->set_bytes_miss(s.second.bytes_miss_);
            e->set_reqs_hit(s.second.reqs_hit_);
            e->set_reqs_miss(s.second.reqs_miss_);
        }
    }

    const auto ct = current_time();
    if (curr_day_ == ct.tm_mday)
    { // Restore the all_stats if it's still the same day
        all_stats.swap(all_stats_);
    }
    else // We've started new day
    {
        set_curr_dump_fpath(ct);
        curr_day_ = ct.tm_mday;
    }

    // Note that this can easily block the io_service thread for a second or
    // few depending on the size of the stats data.
    // Currently it shouldn't be a problem, but in the future ...
    stats_file f(curr_dump_fpath_.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
    if (X3ME_UNLIKELY(!f.is_open()))
    {
        XLOG_ERROR(plgn_tag, "Host_stats. Dump stats failed. Unable to open "
                             "dump stats file '{}'. {}",
                   curr_dump_fpath_,
                   err_code_t{errno, bsys::get_system_category()}.message());
        return;
    }
    if (X3ME_UNLIKELY(!final_stats.SerializeToFileDescriptor(f.fd_)))
    {
        XLOG_ERROR(plgn_tag,
                   "Host_stats. Dump stats failed. Unable to serialize "
                   "dump stats to file '{}'",
                   curr_dump_fpath_);
        return;
    }
    XLOG_DEBUG(plgn_tag, "Host_stats. Dumped stats for {} hosts to {}",
               final_stats.data().size(), curr_dump_fpath_);
}

////////////////////////////////////////////////////////////////////////////////

void host_stats::set_curr_dump_fpath(const std::tm& ctm) noexcept
{
    x3me::utilities::string_builder_128 fpath;
    fpath << settings_.dump_dir_ << '/' << (ctm.tm_year + 1900) << '-'
          << (ctm.tm_mon + 1) << '-' << ctm.tm_mday << ".dat";
    curr_dump_fpath_ = fpath.to_string();
    XLOG_INFO(plgn_tag, "Host_stats. Set current dump file to {}",
              curr_dump_fpath_);
}

std::tm host_stats::current_time() noexcept
{
    std::tm ret   = {};
    const auto ct = ::time(nullptr);
    if (X3ME_UNLIKELY(!::localtime_r(&ct, &ret)))
    {
        XLOG_ERROR(plgn_tag, "Host_stats. Unable to get local time. {}",
                   err_code_t{errno, bsys::get_system_category()}.message());
    }
    return ret;
}

} // namespace plgns
