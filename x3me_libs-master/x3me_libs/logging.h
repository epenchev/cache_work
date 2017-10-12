#pragma once

#include <atomic>

#include "string_builder.h"
#include "utilities.h"

namespace x3me
{
namespace logging
{

struct log_target
{
	virtual ~log_target() noexcept {};
	virtual void write(const char* data, size_t data_length) noexcept = 0;
};

using log_target_ptr_t = std::unique_ptr<log_target>;

// Pass the current log file path and 
// it's expected to return the new log file path
using on_log_rotate_func_t = std::function<std::string (const std::string&)>;

// The sync targets are flushed after every write operation
struct log_target_factory
{
    // The console target is always synced/flushed
	static log_target_ptr_t create_console_log_target() noexcept;
	static log_target_ptr_t create_file_log_target(
            const std::string& file_path, bool truncate) noexcept;
	static log_target_ptr_t create_sync_file_log_target(
            const std::string& file_path, bool truncate) noexcept;
	static log_target_ptr_t create_file_log_rotate_target(
            const std::string& file_path, bool truncate, uint64_t max_file_size, 
            const on_log_rotate_func_t& on_log_rotate_func) noexcept;
	static log_target_ptr_t create_sync_file_log_rotate_target(
            const std::string& file_path, bool truncate, uint64_t max_file_size, 
            const on_log_rotate_func_t& on_log_rotate_func) noexcept;
};

////////////////////////////////////////////////////////////////////////////////

struct logger_data;

class logger
{
public:
	typedef unsigned long long subsystem_flags_t;

private:
	// This max log level could be set/checked from different threads.
	logger_data*	      m_logger_data;
    std::atomic<uint32_t> m_max_log_level{uint32_t(-1)};
	uint32_t		      m_process_id;

	enum {max_count_supported_subsystems = sizeof(subsystem_flags_t) * 8/*bits per byte*/};

private:
	logger();

	void add_log_level(const char* log_level);
	void add_subsystem_type(const char* subsystem);
	void add_log_level(const std::string& log_level)
	{
		add_log_level(log_level.c_str());
	}
	void add_subsystem_type(const std::string& subsystem)
	{
		add_subsystem_type(subsystem.c_str());
	}

	size_t add_log_target(log_target_ptr_t& lt, size_t max_log_level, subsystem_flags_t subsystem_flags);
	bool set_subsystems_for_target(size_t target_idx, subsystem_flags_t subsystem_flags);

	const char* get_header_info(x3me::utilities::string_builder_128& header_sb, size_t log_level, size_t subsystem_type);
	void log_info(size_t log_level, size_t subsystem_type, const char* info, size_t info_size);

	template<typename ItSubsytem>
	subsystem_flags_t get_subsystem_flags(ItSubsytem beg_ss, ItSubsytem end_ss)
	{
		subsystem_flags_t subsystem_flags = 0;
		for(; beg_ss != end_ss; ++beg_ss)
		{
			size_t subsystem_idx = *beg_ss;
			if(subsystem_idx >= max_count_supported_subsystems)
			{
				assert(false);
				continue;
			}

			x3me::utilities::set_bit_by_pos(subsystem_flags, subsystem_idx); // enable subsystem
		}
		return subsystem_flags;
	}

	template<typename Strm> 
	void log(Strm& strm)
	{
		strm << std::endl;
	}
	template<typename Strm, typename Arg0, typename... Args> 
	void log(Strm& strm, const Arg0& arg0, const Args&... args)
	{
		strm << arg0;
		log(strm, args...);
	}

public:
	~logger();

	static logger& get();

	void reset_after_fork();

    // The global log level allows set/get/check from multiple threads
	void set_max_log_level_global(uint32_t log_level)
	{
		m_max_log_level.store(log_level, std::memory_order_release);
	}
	uint32_t get_max_log_level_global() const
	{
		return m_max_log_level.load(std::memory_order_acquire);
	}

	bool set_max_log_level_for_target(size_t target_idx, size_t log_level);

	template<typename ItSubsytem>
	bool set_subsystems_for_target(size_t target_idx, ItSubsytem beg_ss, ItSubsytem end_ss)
	{
		return set_subsystems_for_target(target_idx, get_subsystem_flags(beg_ss, end_ss)); 
	}

	template<typename ItLogLevel, typename ItSubsytem>
	void set_log_levels_and_sybsystems(ItLogLevel beg_ll, ItLogLevel end_ll, ItSubsytem beg_ss, ItSubsytem end_ss) // for const char* and string
	{
		for(; beg_ll != end_ll; ++beg_ll)
		{
			add_log_level(*beg_ll);
		}
		for(size_t subsystem_count = 0; beg_ss != end_ss; ++beg_ss, ++subsystem_count)
		{
			if(subsystem_count >= max_count_supported_subsystems)
			{
				assert(false);
				return;
			}
			add_subsystem_type(*beg_ss);
		}
	}	

	template<typename ItSubsytem>
	size_t add_log_target(log_target_ptr_t& lt, size_t max_log_level, ItSubsytem beg_ss, ItSubsytem end_ss) // will log for chosen subsystems by its idx
	{		
		return add_log_target(lt, max_log_level, get_subsystem_flags(beg_ss, end_ss));
	}	
	size_t add_log_target(log_target_ptr_t& lt, size_t max_log_level) // will log for all subsystems
	{
		return add_log_target(lt, max_log_level, std::numeric_limits<subsystem_flags_t>::max()); // max -> all bits will be set to 1 -> all subsystems are enabled
	}
	void remove_log_target(size_t target_idx);

	template<size_t BufferSize, typename... Args>
	void log(uint32_t log_level, uint32_t subsystem_type, const Args&... args)
	{
		if(log_level > get_max_log_level_global())
			return;
		enum {header_length = 128};	
		x3me::utilities::string_builder<BufferSize+header_length> sb;
		{
			x3me::utilities::string_builder<header_length> header_sb;
			sb << get_header_info(header_sb, log_level, subsystem_type);
		}
		log(sb, args...);
		log_info(log_level, subsystem_type, sb.data(), sb.size());
	}
};

// NOTE_ The below functions will use the stack with given size and if the size is not sufficient, the will use the heap
#define X3ME_LOG_32(log_level, subsystem, ...)		x3me::logging::logger::get().log<32>(log_level, subsystem, __VA_ARGS__)
#define X3ME_LOG_64(log_level, subsystem, ...)		x3me::logging::logger::get().log<64>(log_level, subsystem, __VA_ARGS__)
#define X3ME_LOG_128(log_level, subsystem, ...)		x3me::logging::logger::get().log<128>(log_level, subsystem, __VA_ARGS__)
#define X3ME_LOG_256(log_level, subsystem, ...)		x3me::logging::logger::get().log<256>(log_level, subsystem, __VA_ARGS__)
#define X3ME_LOG_512(log_level, subsystem, ...)		x3me::logging::logger::get().log<512>(log_level, subsystem, __VA_ARGS__)
#define X3ME_LOG_1024(log_level, subsystem, ...)	x3me::logging::logger::get().log<1024>(log_level, subsystem, __VA_ARGS__)
#define X3ME_LOG_2048(log_level, subsystem, ...)	x3me::logging::logger::get().log<2048>(log_level, subsystem, __VA_ARGS__)

} // namespace logging
} // namespace x3me
