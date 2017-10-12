#include "precompiled.h"
#include "cache_error.h"

namespace cache
{

const char* cache_error_category::name() const noexcept
{
    return "cache_error";
}

std::string cache_error_category::message(int err) const noexcept
{
    switch (err)
    {
#define CACHE_ERRORS_IT(var, str)                                              \
    case var:                                                                  \
        return str;

        CACHE_ERRORS(CACHE_ERRORS_IT)

#undef CACHE_ERRORS_IT
    }
    return "Uknown cache error";
}

const cache_error_category& get_cache_error_category() noexcept
{
    static const cache_error_category inst;
    return inst;
}

} // namespace cache
