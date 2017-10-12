#pragma once

namespace cache
{

#define CACHE_ERRORS(MACRO)                                                    \
    MACRO(success, "Success")                                                  \
    MACRO(already_open, "Already open")                                        \
    MACRO(invalid_handle, "Invalid handle")                                    \
    MACRO(eof, "End of file")                                                  \
    MACRO(null_write, "Nothing written")                                       \
    MACRO(operation_aborted, "Operation aborted")                              \
    MACRO(object_present, "Object data already present")                       \
    MACRO(object_in_use, "Object is currently in use")                         \
    MACRO(new_object_too_small, "New object data too small")                   \
    MACRO(object_not_present, "Object data not present")                       \
    MACRO(disk_error, "Disk error")                                            \
    MACRO(corrupted_object_meta, "Corrupted object meta")                      \
    MACRO(corrupted_object_data, "Corrupted object data")                      \
    MACRO(internal_logic_error, "Internal logic error")                        \
    MACRO(service_stopped, "Service stopped")                                  \
    MACRO(unexpected_data, "More data than expected")                          \
    MACRO(tasks_limit_reached, "Cache AIO tasks limit reached")

// Couldn't find appropriate already present system error for these errors.
enum error
{
#define CACHE_ERRORS_IT(var, str) var,

    CACHE_ERRORS(CACHE_ERRORS_IT)

#undef CACHE_ERRORS_IT
};

////////////////////////////////////////////////////////////////////////////////

class cache_error_category final : public boost::system::error_category
{
public:
    const char* name() const noexcept final;

    std::string message(int err) const noexcept final;
};

const cache_error_category& get_cache_error_category() noexcept;

} // namespace cache
////////////////////////////////////////////////////////////////////////////////
// Stuff needed for the error to be recognized by the boost::system machinery.
namespace boost
{
namespace system
{

template <>
struct is_error_code_enum<cache::error>
{
    static const bool value = true;
};

} // namespace system
} // namespace boost
////////////////////////////////////////////////////////////////////////////////
namespace cache
{
inline err_code_t make_error_code(cache::error e)
{
    return err_code_t{static_cast<int>(e), get_cache_error_category()};
}
} // namespace cache
