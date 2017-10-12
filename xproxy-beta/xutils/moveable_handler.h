#pragma once

namespace xutils
{

// This handler provides workaround for the ASIO requirement for copyable
// handlers. There is a compile time check for the handler copy-ability,
// but in some/most cases this copy-ability is not really needed, because
// the handler is never copied inside the ASIO code.
// So using this handler we can bypass the ASIO static check, and the project
// linking will fail if the ASIO functionality actually tries to copy the
// given handler. Note that the copy-ability requirement is already removed
// from the ASIO operations where is not really needed, but the code is
// still not present in the boost::asio version.
template <typename F>
struct moveable_handler : F
{
    moveable_handler(F&& f) : F(std::move(f)) {}

    moveable_handler(moveable_handler&&) = default;
    moveable_handler& operator=(moveable_handler&&) = default;

    moveable_handler(const moveable_handler&);
    moveable_handler& operator=(const moveable_handler&);
};

template <typename F>
auto make_moveable_handler(F&& f)
{
    return moveable_handler<F>(std::forward<F>(f));
}

} // namespace xutils
