#pragma once

namespace cache
{
namespace detail
{

class volume_fd
{
    enum : int
    {
        invalid_fd = -1
    };
    int fd_ = invalid_fd;

public:
    volume_fd() noexcept = default;
    ~volume_fd() noexcept;

    volume_fd(volume_fd&& rhs) noexcept;
    volume_fd& operator=(volume_fd&& rhs) noexcept;

    volume_fd(const volume_fd&) = delete;
    volume_fd& operator=(const volume_fd&) = delete;

    bool open(const char* path, err_code_t& err) noexcept;

    bool read(uint8_t* buf, bytes32_t len, bytes64_t off,
              err_code_t& err) noexcept;
    bool write(const uint8_t* buf, bytes32_t len, bytes64_t off,
               err_code_t& err) noexcept;

    bool truncate(bytes64_t size, err_code_t& err) noexcept;

    bool close(err_code_t& err) noexcept;

    int get() noexcept { return fd_; }
};

} // namespace detail
} // namespace cache
