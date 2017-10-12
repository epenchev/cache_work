#include "types.h"
#include "logging.h"

#include <mutex>
#include <new>

#include "shared_mutex.h"
#include "sys_utils.h"
#include "utils.h"

namespace x3me
{
namespace logging
{

using namespace x3me::types;

////////////////////////////////////////////////////////////////////////////////

class localtime_io
{
    tm t_;

public:
    localtime_io() noexcept
    {
        const auto t = ::time(nullptr);
        ::localtime_r(&t, &t_);
    }

    friend std::ostream& operator<<(std::ostream& os,
                                    const localtime_io& rhs) noexcept
    {
        os << 1900 + rhs.t_.tm_year << '-';
        print_escape(os, rhs.t_.tm_mon + 1, '-');
        print_escape(os, rhs.t_.tm_mday, ' ');
        print_escape(os, rhs.t_.tm_hour, ':');
        print_escape(os, rhs.t_.tm_min, ':');
        if (rhs.t_.tm_sec < 10)
            os << '0';
        os << rhs.t_.tm_sec;

        return os;
    }

private:
    static void print_escape(std::ostream& os, int n, char end) noexcept
    {
        if (n < 10)
            os << '0';
        os << n << end;
    }
};

////////////////////////////////////////////////////////////////////////////////

struct log_policy
{
    static FILE* open(const char* file_path, bool truncate) noexcept
    {
        auto file = fopen(file_path, (truncate ? "wb" : "ab"));
        X3ME_ENFORCE(file);
        return file;
    }
    static void write(FILE* file, const char* data, size_t size) noexcept
    {
        fwrite(data, sizeof(char), size, file);
    }
    static void close(FILE*& file) noexcept
    {
        if (file)
        {
            fclose(file);
            file = nullptr;
        }
    }
};

struct sync_log_policy : log_policy
{
    static void write(FILE* file, const char* data, size_t size) noexcept
    {
        log_policy::write(file, data, size);
        fflush(file);
    }
};

////////////////////////////////////////////////////////////////////////////////

struct console_log_target final : public log_target
{
    virtual ~console_log_target() noexcept final {}

    virtual void write(const char* data, size_t size) noexcept final
    {
        fwrite(data, sizeof(char), size, stdout);
        fflush(stdout); // console is flushed for better visual effect
    }
};

template <typename Policy>
class file_log_target final : public log_target
{
private:
    FILE* file_;

public:
    file_log_target(const char* file_path, bool truncate) noexcept
        : file_(Policy::open(file_path, truncate))
    {
    }

    virtual ~file_log_target() noexcept final { Policy::close(file_); }

    virtual void write(const char* data, size_t size) noexcept final
    {
        Policy::write(file_, data, size);
    }
};

template <typename Policy>
class file_log_rotate_target final : public log_target
{
private:
    FILE* file_;
    thread::shared_mutex mutex_;
    std::atomic_ullong log_size_;
    const uint64_t max_log_size_;
    std::string log_file_path_;
    on_log_rotate_func_t log_rotate_func_;

public:
    file_log_rotate_target(const char* file_path, bool truncate,
                           uint64_t max_log_size,
                           const on_log_rotate_func_t& log_rotate_func) noexcept
        : file_(Policy::open(file_path, truncate)),
          log_size_(0),
          max_log_size_(max_log_size),
          log_file_path_(file_path),
          log_rotate_func_(log_rotate_func)
    {
    }

    virtual ~file_log_rotate_target() noexcept final { Policy::close(file_); }

    virtual void write(const char* data, size_t size) noexcept final
    {
        // The current functionality is not precise, but there is no
        // such requirement. A log file can get bigger than the max size
        // before it gets rotated. This simplifies a lot the locking here.
        bool rotate = false;
        {
            x3me::thread::shared_lock _(mutex_);
            Policy::write(file_, data, size);
            auto prev = log_size_.fetch_add(size, std::memory_order_relaxed);
            rotate    = ((prev + size) >= max_log_size_);
        }
        if (rotate)
        { // Several threads can end up waiting to rotate file.
            // That's why we need the below check. We want to rotate the file
            // only once when it's needed.
            std::lock_guard<thread::shared_mutex> _(mutex_);
            if (log_size_.load(std::memory_order_acquire) >= max_log_size_)
            {
                Policy::close(file_);
                // Report finished log and get new log file path
                log_file_path_ = log_rotate_func_(log_file_path_);
                file_ = Policy::open(log_file_path_.c_str(), true /*truncate*/);
                log_size_.store(0ULL, std::memory_order_release);
            }
        }
    }
};

////////////////////////////////////////////////////////////////////////////////

log_target_ptr_t log_target_factory::create_console_log_target() noexcept
{
    return log_target_ptr_t(new (std::nothrow) console_log_target);
}

log_target_ptr_t
log_target_factory::create_file_log_target(const std::string& file_path,
                                           bool truncate) noexcept
{
    return log_target_ptr_t(new (std::nothrow) file_log_target<log_policy>(
        file_path.c_str(), truncate));
}

log_target_ptr_t
log_target_factory::create_sync_file_log_target(const std::string& file_path,
                                                bool truncate) noexcept
{
    return log_target_ptr_t(new (std::nothrow) file_log_target<sync_log_policy>(
        file_path.c_str(), truncate));
}

log_target_ptr_t log_target_factory::create_file_log_rotate_target(
    const std::string& file_path, bool truncate, uint64_t max_file_size,
    const on_log_rotate_func_t& on_log_rotate_func) noexcept
{
    return log_target_ptr_t(
        new (std::nothrow) file_log_rotate_target<log_policy>(
            file_path.c_str(), truncate, max_file_size, on_log_rotate_func));
}

log_target_ptr_t log_target_factory::create_sync_file_log_rotate_target(
    const std::string& file_path, bool truncate, uint64_t max_file_size,
    const on_log_rotate_func_t& on_log_rotate_func) noexcept
{
    return log_target_ptr_t(
        new (std::nothrow) file_log_rotate_target<sync_log_policy>(
            file_path.c_str(), truncate, max_file_size, on_log_rotate_func));
}

////////////////////////////////////////////////////////////////////////////////

struct logger_data
{
    struct log_target_info
    {
        logger::subsystem_flags_t enabled_subsystems;
        size_t max_log_level;
        log_target_ptr_t log_target;

        log_target_info(log_target_ptr_t& lt, size_t mll,
                        logger::subsystem_flags_t ssf)
            : enabled_subsystems(ssf)
            , max_log_level(mll)
            , log_target(std::move(lt))
        {
        }
        log_target_info(log_target_info&& lti)
            : enabled_subsystems(lti.enabled_subsystems)
            , max_log_level(lti.max_log_level)
            , log_target(std::move(lti.log_target))
        {
        }
        log_target_info& operator=(log_target_info&& lti)
        {
            if (this != &lti)
            {
                enabled_subsystems = lti.enabled_subsystems;
                max_log_level      = lti.max_log_level;
                log_target         = std::move(lti.log_target);
            }
            return *this;
        }

    private:
        log_target_info(const log_target_info&);
        log_target_info& operator=(const log_target_info&);
    };
    typedef vector_t<log_target_info>::type log_targets_array;

    size_t max_length_log_level;
    size_t max_length_system_type;

    string_array_t log_levels;
    string_array_t subsystems_type;
    log_targets_array log_targets;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

logger::logger()
    : m_logger_data(X3ME_NEW(logger_data))
    , m_process_id(x3me::sys_utils::process_id())
{
    m_logger_data->max_length_log_level   = 0;
    m_logger_data->max_length_system_type = 0;
}

logger::~logger()
{
    X3ME_DELETE(m_logger_data);
}

logger& logger::get()
{
    static logger l;
    return l;
}

void logger::reset_after_fork()
{
    m_process_id = x3me::sys_utils::process_id();
}

bool logger::set_max_log_level_for_target(size_t target_idx, size_t log_level)
{
    size_t count_log_targets = m_logger_data->log_targets.size();
    if (target_idx >= count_log_targets)
    {
        assert(false);
        return false;
    }
    m_logger_data->log_targets[target_idx].max_log_level = log_level;
    return true;
}

void logger::add_log_level(const char* log_level)
{
    size_t length_log_level = ::strlen(log_level);
    if (length_log_level > m_logger_data->max_length_log_level)
    {
        m_logger_data->max_length_log_level = length_log_level;
    }
    m_logger_data->log_levels.push_back(string_t(log_level));
}

void logger::add_subsystem_type(const char* subsystem)
{
    size_t length_subsystem = ::strlen(subsystem);
    if (length_subsystem > m_logger_data->max_length_system_type)
    {
        m_logger_data->max_length_system_type = length_subsystem;
    }
    m_logger_data->subsystems_type.push_back(string_t(subsystem));
}

size_t logger::add_log_target(log_target_ptr_t& lt, size_t max_log_level,
                              subsystem_flags_t subsystem_flags)
{
    if (lt)
    {
        m_logger_data->log_targets.push_back(
            logger_data::log_target_info(lt, max_log_level, subsystem_flags));
        return (m_logger_data->log_targets.size() - 1);
    }
    assert(lt);
    return -1;
}

void logger::remove_log_target(size_t target_idx)
{
    size_t count_log_targets = m_logger_data->log_targets.size();
    if (target_idx < count_log_targets)
    {
        auto it = m_logger_data->log_targets.begin();
        std::advance(it, target_idx);
        m_logger_data->log_targets.erase(it);
    }
    else
    {
        assert(false);
    }
}

bool logger::set_subsystems_for_target(size_t target_idx,
                                       subsystem_flags_t subsystem_flags)
{
    size_t count_log_targets = m_logger_data->log_targets.size();
    if (target_idx >= count_log_targets)
    {
        assert(false);
        return false;
    }
    m_logger_data->log_targets[target_idx].enabled_subsystems = subsystem_flags;
    return true;
}

const char*
logger::get_header_info(x3me::utilities::string_builder_128& header_sb,
                        size_t log_level, size_t subsystem_type)
{
    constexpr auto delimiter = " | ";
    const auto thread_id     = x3me::sys_utils::thread_id();

    header_sb << std::setfill(' ') << std::fixed;
    // date-time
    header_sb << localtime_io() << delimiter;
    // process id
    header_sb << m_process_id << delimiter;
    // thread id
    header_sb << thread_id << delimiter;
    // log level
    header_sb << std::setw(m_logger_data->max_length_log_level)
              << m_logger_data->log_levels[log_level] << delimiter;
    // subsystem
    header_sb << std::setw(m_logger_data->max_length_system_type)
              << m_logger_data->subsystems_type[subsystem_type] << delimiter;

    // little hack. We set '\0' at the end of the stream to treat its data as
    // zero terminated string
    header_sb << '\0';

    return header_sb.data();
}

void logger::log_info(size_t log_level, size_t subsystem_type, const char* info,
                      size_t info_size)
{
    logger_data::log_targets_array& lta = m_logger_data->log_targets;
    for (auto it = lta.begin(); it != lta.end(); ++it)
    {
        logger_data::log_target_info& lti = *it;
        // if log level is less and subsystem is enabled for logging on this
        // target
        if ((log_level <= lti.max_log_level) &&
            x3me::utilities::bit_set_by_pos(lti.enabled_subsystems,
                                            subsystem_type))
        {
            lti.log_target->write(info, info_size);
        }
    }
}

} // namespace logging
} // namespace x3me
