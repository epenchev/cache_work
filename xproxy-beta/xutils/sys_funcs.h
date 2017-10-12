#pragma once

namespace xutils
{

/// Sets the max count of opened file descriptors for the process.
/// Returns true in case of success and false otherwise.
/// In case of error the 'err' is filled with info about it.
bool set_max_count_fds(uint32_t cnt, err_code_t& err);

} // namespace xutils
