#pragma once

#include <type_traits>

namespace x3me
{
namespace utils
{

template <typename T>
class mem_fn_delegate;

template <typename Result, typename... Args>
class mem_fn_delegate<Result(Args...)>
{
    using stub_ptr_t = Result (*)(void*, Args...);

    void* obj_       = nullptr;
    stub_ptr_t stub_ = nullptr;

private:
    mem_fn_delegate(void* obj, stub_ptr_t stub) : obj_(obj), stub_(stub) {}

    template <typename Class, Result (Class::*Method)(Args...)>
    static Result stub(void* obj, Args... args)
    {
        return (static_cast<Class*>(obj)->*Method)(std::forward<Args>(args)...);
    }
    template <typename Class, Result (Class::*Method)(Args...) const>
    static Result stub(void* obj, Args... args)
    {
        return (static_cast<const Class*>(obj)->*Method)(
            std::forward<Args>(args)...);
    }

public:
    mem_fn_delegate() = default;

    template <typename Class, Result (Class::*Method)(Args...)>
    static mem_fn_delegate create(Class* obj)
    {
        return mem_fn_delegate{obj, &stub<Class, Method>};
    }
    template <typename Class, Result (Class::*Method)(Args...) const>
    static mem_fn_delegate create(Class* obj)
    {
        return mem_fn_delegate{obj, &stub<Class, Method>};
    }

    template <typename Class, Result (Class::*Method)(Args...)>
    void assign(Class* obj)
    {
        obj_  = obj;
        stub_ = &stub<Class, Method>;
    }
    template <typename Class, Result (Class::*Method)(Args...) const>
    void assign(Class* obj)
    {
        obj_  = obj;
        stub_ = &stub<Class, Method>;
    }

    Result operator()(Args... args)
    {
        return (*stub_)(obj_, std::forward<Args>(args)...);
    }

    Result operator()(Args... args) const
    {
        return (*stub_)(obj_, std::forward<Args>(args)...);
    }

    explicit operator bool() const { return !!obj_; }
};

#define make_mem_fn_delegate(method_ptr, obj_ptr)                              \
    decltype(x3me::utils::detail::deduce_type(                                 \
        method_ptr))::create<std::remove_pointer_t<decltype(obj_ptr)>,         \
                             method_ptr>(obj_ptr)

////////////////////////////////////////////////////////////////////////////////
namespace detail
{

template <typename Ret, typename Class, typename... Args>
mem_fn_delegate<Ret(Args...)> deduce_type(Ret (Class::*)(Args...));

template <typename Ret, typename Class, typename... Args>
mem_fn_delegate<Ret(Args...)> deduce_type(Ret (Class::*)(Args...) const);

} // namespace detail
} // namespace utils
} // namespace x3me
