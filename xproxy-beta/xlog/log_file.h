#pragma once

namespace xlog
{
namespace detail
{
using hdr_data_t = std::array<iovec, 2>;

inline auto full_size(const hdr_data_t& hd) noexcept
{
    static_assert(x3me::utils::size(hdr_data_t{}) == 2, "");
    return hd[0].iov_len + hd[1].iov_len;
}

////////////////////////////////////////////////////////////////////////////////

class log_file
{
    static constexpr int invalid_fd = -1;

    int fd_ = invalid_fd;

public:
    log_file() noexcept = default;
    ~log_file() noexcept;

    log_file(log_file&& rhs) noexcept;

    log_file(const log_file&) = delete;
    log_file& operator=(const log_file&) = delete;
    log_file& operator=(log_file&&) = delete;

    bool open(const char* file_path, bool truncate, err_code_t& err) noexcept;
    bool close(err_code_t& err) noexcept;

    bool write(const char* data, uint32_t size, err_code_t& err) noexcept;
    bool write(hdr_data_t& data, err_code_t& err) noexcept;

    int64_t size(err_code_t& err) const noexcept;
    uint32_t block_size(err_code_t& err) const noexcept;

    /// The OS API which is called inside this success only if the
    /// beg and size arguments are modulo of the block_size.
    /// In addition, by some reason, the OS API doesn't allow removing
    /// from the end of the file.
    /// N.B.!!! Writing after remove range if the file is not opened in append
    /// mode leads to undefined behavior - the write succeeds but there is
    /// garbage in the file.
    bool remove_range(uint64_t beg, uint64_t size, err_code_t& err) noexcept;

    bool valid() const noexcept { return fd_ != invalid_fd; }
    explicit operator bool() const noexcept { return valid(); }
};

} // namespace detail
} // namespace xlog
