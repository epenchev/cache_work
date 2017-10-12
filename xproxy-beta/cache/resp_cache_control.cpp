#include "precompiled.h"
#include "resp_cache_control.h"

namespace cache
{

std::ostream& operator<<(std::ostream& os,
                         const resp_cache_control& rhs) noexcept
{
    switch (rhs)
    {
#define XX(name)                                                               \
    case resp_cache_control::name:                                             \
        return os << #name;
        RESP_CACHE_CONTROL(XX)
#undef XX
    }
    return os;
}

} // namespace cache
