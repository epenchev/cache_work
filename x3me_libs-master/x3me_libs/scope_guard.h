#pragma once

#include <utility>

namespace x3me
{
namespace utils
{

template <typename Func>
class scope_guard
{
	Func func_;
	bool active_;

public:
	explicit scope_guard(const Func& func)
	: func_(func)
	, active_(true)
	{
	}
	explicit scope_guard(Func&& func)
	: func_(std::move(func))
	, active_(true)
	{
	}
	scope_guard(scope_guard&& rhs)
	: func_(std::move(rhs.func_))
	, active_(rhs.active_)
	{
		rhs.dismiss();	
	}
	~scope_guard() { if (active_) func_(); }

	void dismiss() { active_ = false; }

	scope_guard() = delete;
	scope_guard(const scope_guard&) = delete;
	scope_guard& operator =(const scope_guard&) = delete;
	scope_guard& operator =(scope_guard&&) = delete;
};

////////////////////////////////////////////////////////////////////////////////

template <typename Func>
auto make_scope_guard(Func&& func)
{
	return scope_guard<Func>(std::forward<Func>(func));
}

////////////////////////////////////////////////////////////////////////////////
// The code here is needed only for the usage of the simple scope guard 
// through a macro without need to name any variable.

namespace detail
{
	enum class scope_guard_on_exit {};

	template <typename Func>
	auto operator +(scope_guard_on_exit, Func&& func)
	{
		return scope_guard<Func>(std::forward<Func>(func));
	}
}

} // namespace utils
} // namespace x3me

#define X3ME_CONCAT_STR_IMPL(s1, s2) s1##s2
#define X3ME_CONCAT_STR(s1, s2) X3ME_CONCAT_STR_IMPL(s1, s2)
#define X3ME_ANONYMOUS_VAR(s) X3ME_CONCAT_STR(s, __COUNTER__)
// Example usage:
// auto p = malloc(1024);
// X3ME_SCOPE_EXIT
// {
//     delete p; p = nullptr;
// };
#define X3ME_SCOPE_EXIT \
	auto X3ME_ANONYMOUS_VAR(x3me_scope_exit_var) \
	= x3me::utils::detail::scope_guard_on_exit() + [&]()
