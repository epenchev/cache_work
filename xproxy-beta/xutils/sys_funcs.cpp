#include "precompiled.h"
#include "sys_funcs.h"

namespace xutils
{

bool set_max_count_fds(uint32_t cnt, err_code_t& err)
{
    using boost::system::system_category;

    rlimit cur_lims = {0};
    if (getrlimit(RLIMIT_NOFILE, &cur_lims) != 0)
    {
        err.assign(errno, system_category());
        return false;
    }

    rlimit new_lims   = cur_lims;
    new_lims.rlim_cur = cnt;
    // Don't try to change the hard limit if it's big enough.
    if (cnt > new_lims.rlim_max)
        new_lims.rlim_max = cnt;
    if (setrlimit(RLIMIT_NOFILE, &new_lims) != 0)
    {
        err.assign(errno, system_category());
        return false;
    }

    return true;
}

} // namespace xutils
