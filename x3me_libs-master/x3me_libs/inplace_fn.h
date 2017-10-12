#pragma once

#include <type_traits>

#include "mpl.h"
#include "perf_hint.h"
#include "utils.h"

namespace x3me
{
namespace utils
{

template <size_t Size, size_t Align = alignof(void*)>
struct inplace_params
{
    enum : size_t
    {
        size  = Size,
        align = Align,
    };
};

template <typename Params, typename Signature>
class inplace_fn;

template <typename Params, typename Ret, typename... Args>
class inplace_fn<Params, Ret(Args...)>
{
    template <typename Fun>
    using is_callable = std::integral_constant<
        bool, std::is_same<std::result_of_t<Fun(Args...)>, Ret>::value>;
    template <typename Fun>
    using is_inplace_fn = std::is_same<std::decay_t<Fun>, inplace_fn>;

private:
    struct vtable
    {
        void (*destroy)(void* this_ptr) noexcept;
        void (*copy_to)(const void* this_ptr, void* to);
        void (*move_to)(void* this_ptr, void* to);
        Ret (*invoke_const)(const void* this_ptr, Args...);
        Ret (*invoke)(void* this_ptr, Args...);
    };

    template <typename Fn>
    struct fn
    {
        static vtable vtbl;

        Fn fn_;

        template <typename F>
        explicit fn(F&& f)
            : fn_(std::forward<F>(f))
        {
        }

        static void destroy(void* this_ptr) noexcept
        {
            static_cast<fn*>(this_ptr)->~fn();
        }

        static void copy_to(const void* this_ptr, void* to)
        {
            new (to) fn(*static_cast<const fn*>(this_ptr));
        }

        static void move_to(void* this_ptr, void* to)
        {
            new (to) fn(std::move(*static_cast<fn*>(this_ptr)));
            // As far as I know (and checked) the std::function destroys
            // the moved-from functor object when the std::function is moved.
            // We behave in the same way.
            static_cast<fn*>(this_ptr)->~fn();
        }

        static Ret invoke_const(const void* this_ptr, Args... args)
        {
            return static_cast<const fn*>(this_ptr)
                ->fn_(std::forward<Args>(args)...);
        }

        static Ret invoke(void* this_ptr, Args... args)
        {
            return static_cast<fn*>(this_ptr)->fn_(std::forward<Args>(args)...);
        }
    };

private:
    static_assert(Params::align >= alignof(vtable*), "Insufficient alignment");
    using storage_t = std::aligned_storage_t<Params::size, Params::align>;

    vtable* vptr_ = nullptr;
    storage_t storage_;

public:
    inplace_fn() noexcept {}

    inplace_fn(std::nullptr_t) noexcept : inplace_fn() {}

    ~inplace_fn() noexcept { clear(); }

    inplace_fn(const inplace_fn& rhs) { copy_from(rhs); }

    inplace_fn(inplace_fn&& rhs) { move_from(rhs); }

    template <typename Fun,
              typename = std::enable_if_t<is_callable<Fun>::value>,
              typename = std::enable_if_t<!is_inplace_fn<Fun>::value>>
    inplace_fn(Fun&& fun)
    {
        set(std::forward<Fun>(fun));
    }

    inplace_fn& operator=(std::nullptr_t) noexcept
    {
        clear();
        return *this;
    }

    inplace_fn& operator=(const inplace_fn& rhs)
    {
        if (X3ME_LIKELY(this != &rhs))
        {
            clear();
            copy_from(rhs);
        }
        return *this;
    }

    inplace_fn& operator=(inplace_fn&& rhs)
    {
        if (X3ME_LIKELY(this != &rhs))
        {
            clear();
            move_from(rhs);
        }
        return *this;
    }

    template <typename Fun,
              typename  = std::enable_if_t<is_callable<Fun>::value>,
              typename  = std::enable_if_t<!is_inplace_fn<Fun>::value>>
    inplace_fn& operator=(Fun&& fun)
    {
        clear();
        set(std::forward<Fun>(fun));
        return *this;
    }

    void swap(inplace_fn& rhs)
    {
        inplace_fn tmp;
        tmp.move_from(rhs);
        rhs.move_from(*this);
        move_from(tmp);
    }

    Ret operator()(Args... args)
    {
        return vptr_->invoke(&storage_, std::forward<Args>(args)...);
    }

    Ret operator()(Args... args) const
    {
        return vptr_->invoke_const(&storage_, std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept { return !!vptr_; }

private:
    void copy_from(const inplace_fn& rhs)
    {
        if (rhs.vptr_)
            rhs.vptr_->copy_to(&rhs.storage_, &storage_);
        vptr_ = rhs.vptr_;
    }

    void move_from(inplace_fn& rhs)
    {
        if (rhs.vptr_)
            rhs.vptr_->move_to(&rhs.storage_, &storage_);
        vptr_     = rhs.vptr_;
        rhs.vptr_ = nullptr;
    }

    template <typename Fun>
    void set(Fun&& fun)
    {
        using fn_t = fn<Fun>;
        X3ME_STATIC_NUM_CHECK(sizeof(storage_t) == sizeof(fn_t),
                              sizeof(storage_t), sizeof(fn_t));
        X3ME_STATIC_NUM_CHECK(alignof(storage_t) == alignof(fn_t),
                              alignof(storage_t), alignof(fn_t));
        new (&storage_) fn_t(std::forward<Fun>(fun));
        vptr_ = &fn_t::vtbl;
    }

    void clear() noexcept
    {
        if (vptr_)
            vptr_->destroy(&storage_);
        vptr_ = nullptr;
    }
};

// clang-format off
template <typename Params, typename Ret, typename... Args>
template <typename Fn>
typename inplace_fn<Params, Ret(Args...)>::vtable 
inplace_fn<Params, Ret(Args...)>::fn<Fn>::vtbl
{
    inplace_fn<Params, Ret(Args...)>::fn<Fn>::destroy,
    inplace_fn<Params, Ret(Args...)>::fn<Fn>::copy_to,
    inplace_fn<Params, Ret(Args...)>::fn<Fn>::move_to,
    inplace_fn<Params, Ret(Args...)>::fn<Fn>::invoke_const,
    inplace_fn<Params, Ret(Args...)>::fn<Fn>::invoke
};
// clang-format on

} // namespace utils
} // namespace x3me
