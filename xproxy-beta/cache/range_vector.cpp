#include "precompiled.h"
#include "range_vector.h"

namespace cache
{
namespace detail
{

range_vector::range_vector() noexcept
{
    set_empty_data();
}

range_vector::range_vector(const range_elem& rhs) noexcept
{
    // Note that we preserve the range_elem metadata here.
    ::memcpy(data_, &rhs, sizeof(rhs));
    get_range_elem()->set_mark();
}

range_vector::~range_vector() noexcept
{
    if (has_data())
        destroy_data();
}

range_vector::range_vector(const range_vector& rhs) noexcept
{
    if (rhs.has_data())
        copy_data(rhs);
    else
        copy_range_elem(rhs);
}

range_vector& range_vector::operator=(const range_vector& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        if (has_data())
            destroy_data();
        if (rhs.has_data())
            copy_data(rhs);
        else
            copy_range_elem(rhs);
    }
    return *this;
}

range_vector::range_vector(range_vector&& rhs) noexcept
{
    if (rhs.has_data())
        move_data(rhs);
    else
        move_range_elem(rhs);
}

range_vector& range_vector::operator=(range_vector&& rhs) noexcept
{
    if (X3ME_LIKELY(this != &rhs))
    {
        if (has_data())
            destroy_data();
        if (rhs.has_data())
            move_data(rhs);
        else
            move_range_elem(rhs);
    }
    return *this;
}

////////////////////////////////////////////////////////////////////////////////

std::pair<range_vector::const_iterator, bool>
range_vector::add_range(const range_elem& rng) noexcept
{
    // NOTE It's pretty important that we add range_elem here and we preserve
    // the current readers for this range_elem.
    std::pair<range_vector::const_iterator, bool> ret(nullptr, false);
    const auto s = size();
    switch (s)
    {
    case 0: // Use the small buffer optimization
        ::memcpy(data_, &rng, sizeof(range_elem));
        get_range_elem()->set_mark();
        ret.first  = get_range_elem();
        ret.second = true;
        break;
    case 1: // We need to go on the heap
    {
        X3ME_ENFORCE(!has_data(), "SBO must be used for 1 entry");
        auto* r = get_range_elem();
        using x3me::math::ranges_overlap;
        if (!ranges_overlap(r->rng_offset(), r->rng_end_offset(),
                            rng.rng_offset(), rng.rng_end_offset()))
        {
            range_elem tmp;
            ::memcpy(&tmp, data_, sizeof(range_elem));
            set_empty_data();
            auto* d  = get_data();
            d->ptr_  = ::malloc(sizeof(range_elem) * 2);
            d->size_ = 1;
            ::memcpy(d->ptr_, &tmp, sizeof(range_elem));
            ret = add_range_impl(rng, 2);
            X3ME_ENFORCE(
                ret.first && ret.second,
                "The add must succeed because we don't have an overlap");
        }
        else
        {
            ret.first  = r;
            ret.second = false;
        }
        break;
    }
    case max_ranges:
        // We shouldn't add more ranges than the logical limit.
        break;
    default: // Find the position to insert the new element on the heap
        ret = add_range_impl(rng, s);
        X3ME_ASSERT(ret.first, "The call must return some iterator, no matter "
                               "if it succeeds or fails");
        break;
    }
    return ret;
}

range_vector::iter_range range_vector::find_full_range(range rng) const noexcept
{
    X3ME_ASSERT(!rng.empty(), "Doesn't work with empty range");

    const auto rng_offs = rng.beg();
    const auto rng_size = rng.len();
    const auto beg      = cbegin();
    const auto end      = cend();

    iter_range ret(end, end);
    auto it = std::lower_bound(beg, end, rng);
    if (it != end)
    {
        if ((it->rng_offset() > rng_offs) && (it != beg))
            --it;
        if (x3me::math::in_range(rng_offs, it->rng_offset(),
                                 it->rng_end_offset()))
        {
            const auto erng_offs = rng_offs + rng_size;
            auto last_end        = it->rng_offset();
            auto it2 = it;
            for (; it2 != end; ++it2)
            {
                const auto offs  = it2->rng_offset();
                const auto eoffs = it2->rng_end_offset();
                X3ME_ASSERT(offs >= last_end,
                            "Must not have overlapped ranges");
                if (offs > last_end)
                {
                    it2 = end;
                    break; // We have a hole
                }
                if (eoffs >= erng_offs)
                    break; // We found the full range
                last_end = eoffs;
            }
            if (it2 != end)
                ret = iter_range(it, it2 + 1);
        }
    }
    // The searched range may fit entirely in the last range element.
    else if (beg < end)
    {
        auto l = end - 1;
        if (x3me::math::in_range(rng_offs, rng_offs + rng_size, l->rng_offset(),
                                 l->rng_end_offset()))
        {
            ret = iter_range(l, end);
        }
    }
    return ret;
}

range_vector::iter_range range_vector::find_exact_range(range rng) const
    noexcept
{
    auto ret = find_full_range(rng);
    if (ret)
    {
        if ((ret.front().rng_offset() != rng.beg()) ||
            (ret.back().rng_end_offset() != rng.end()))
        { // The found range is not exact
            const auto end = cend();
            ret            = iter_range(end, end);
        }
    }
    return ret;
}

range_vector::const_iterator
range_vector::find_exact_range(const range_elem& rng) const noexcept
{
    const auto beg = cbegin();
    const auto end = cend();
    auto it = std::lower_bound(beg, end, rng);
    return ((it != end) && (it->rng_offset() == rng.rng_offset()) &&
            (it->rng_size() == rng.rng_size()))
               ? it
               : end;
}

range_vector::iter_range range_vector::find_in_range(range rng) const noexcept
{
    X3ME_ASSERT(!rng.empty(), "Doesn't work with empty range");

    const auto rng_offs = rng.beg();
    const auto rng_size = rng.len();
    const auto beg      = cbegin();
    const auto end      = cend();

    iter_range ret(end, end);

    const auto erng_offs = rng_offs + rng_size;
    auto it = std::lower_bound(beg, end, rng);
    if (it != end)
    {
        if ((it->rng_offset() > rng_offs) && (it != beg) &&
            (rng_offs < std::prev(it)->rng_end_offset()))
        {
            --it;
        }
        if (x3me::math::ranges_overlap(rng_offs, erng_offs, it->rng_offset(),
                                       it->rng_end_offset()))
        {
            auto last_end = it->rng_offset();
            auto it2 = it;
            for (; it2 != end; ++it2)
            {
                const auto offs  = it2->rng_offset();
                const auto eoffs = it2->rng_end_offset();
                X3ME_ASSERT(offs >= last_end,
                            "Must not have overlapped ranges");
                if (eoffs >= erng_offs)
                    break; // We found the full range
                last_end = eoffs;
            }
            if ((it2 != end) && (it2->rng_offset() >= erng_offs))
            {
                X3ME_ASSERT(it2 != it, "Wrong logic in the function here");
                --it2;
            }
            ret = iter_range(it, ((it2 != end) ? (it2 + 1) : it2));
        }
    }
    // The searched element may overlap with the end of the last element.
    else if (beg < end)
    {
        auto l = end - 1;
        if (x3me::math::in_range(rng_offs, l->rng_offset(),
                                 l->rng_end_offset()))
        {
            ret = iter_range(l, end);
        }
    }
    return ret;
}

bool range_vector::are_continuous(iter_range rngs) noexcept
{
    X3ME_ASSERT(!rngs.empty(), "Doesn't work with empty range");
    auto prev_end = rngs.front().rng_end_offset();
    for (const auto& r : rngs)
    {
        if (r.rng_offset() > prev_end)
            return false;
        prev_end = r.rng_end_offset();
    }
    return true;
}

range range_vector::trim_overlaps(range rng) const noexcept
{
    // The assertion against the range emptiness is inside the find_in_range.
    if (auto rngs = find_in_range(rng))
    {
        // Fast return if the found range is inside the requested one
        if ((rng.beg() < rngs.front().rng_offset()) &&
            (rngs.back().rng_end_offset() < rng.end()))
            return rng;

        // Trims to the element before the first hole
        auto trim_to_first_hole = [](auto& rngs)
        {
            auto prev = rngs.begin();
            for (auto it = prev + 1; it != rngs.end(); ++it, ++prev)
            {
                if (it->rng_offset() > prev->rng_end_offset())
                {
                    rngs = iter_range{prev, rngs.end()};
                    return prev->rng_end_offset();
                }
            }
            rngs = iter_range{}; // Consume all. No holes.
            return prev->rng_end_offset();
        };
        // Trims to the element after the last hole
        auto trim_to_last_hole = [](const auto& rngs)
        {
            // rngs is not empty
            auto prev_beg = std::prev(rngs.end())->rng_end_offset();
            for (auto it = rngs.end() - 1;; --it)
            {
                if (prev_beg > it->rng_end_offset())
                    break;
                prev_beg = it->rng_offset();
                if (it == rngs.begin())
                    break;
            }
            return prev_beg;
        };

        auto rng_beg = rng.beg();
        auto rng_end = rng.end();
        if (rngs.front().rng_offset() <= rng_beg)
        { // We can trim from the beginning
            const auto new_beg = trim_to_first_hole(rngs);
            if (new_beg < rng_end)
                rng_beg = new_beg;
            else
                rng_beg = rng_end;
        }
        if (((rng_end - rng_beg) >= cache::detail::min_obj_size) &&
            !rngs.empty() && (rngs.back().rng_end_offset() >= rng.end()))
        { // We can trim from the end
            const auto new_end = trim_to_last_hole(rngs);
            X3ME_ENFORCE(x3me::math::in_range(new_end, rng_beg, rng_end),
                         "Wrong logic searching the hole from the end");
            rng_end = new_end;
        }
        if ((rng_end - rng_beg) >= cache::detail::min_obj_size)
            rng = range{rng_beg, rng_end - rng_beg};
        else
            rng = range{};
    }
    return rng;
}

range_vector::const_iterator range_vector::rem_range(iter_range rng) noexcept
{
    const_iterator ret = nullptr;
    X3ME_ASSERT(!rng.empty(), "Must not pass empty ranges here");

    const auto beg   = rng.begin();
    const auto end   = rng.end();
    const auto c_beg = cbegin();
    const auto c_end = cend();
    X3ME_ASSERT(
        x3me::math::ranges_overlap(reinterpret_cast<uintptr_t>(beg),
                                   reinterpret_cast<uintptr_t>(end),
                                   reinterpret_cast<uintptr_t>(c_beg),
                                   reinterpret_cast<uintptr_t>(c_end)) ==
            (rng.size() * sizeof(range_elem)),
        "The range for removing must lay inside the current memory range");

    const size_t size = c_end - c_beg;
    switch (size)
    {
    case 0:
        ret = c_end; // Nothing to do for this case
        break;
    case 1:
        X3ME_ASSERT(!has_data(), "The SBO must have kicked-in");
        set_empty_data(); // Remove the single range element
        ret = cend(); // The end is changed we need to call the function
        break;
    default:
    {
        X3ME_ASSERT(has_data(), "We must be on the heap");
        auto* d               = get_data();
        const size_t rem_size = end - beg;
        const size_t new_size = size - rem_size;
        switch (new_size)
        {
        case 0:
            destroy_data();
            set_empty_data();
            ret = cend();
            break;
        case 1:
        {
            const bool next_end = c_beg < beg;
            // The remaining element is either at the beginning, or at the end.
            range_elem tmp;
            if (c_beg < beg)
                ::memcpy(&tmp, c_beg, sizeof(range_elem));
            else
            {
                X3ME_ASSERT((c_end - end) == 1,
                            "Must be one element at the end");
                ::memcpy(&tmp, end, sizeof(range_elem));
            }
            destroy_data();
            ::memcpy(data_, &tmp, sizeof(tmp));
            X3ME_ASSERT(!has_data(), "SBO must kicked-in");
            ret = next_end ? cend() : cbegin();
            break;
        }
        default:
        {
            const auto pos = beg - c_beg;
            auto p         = static_cast<range_elem*>(d->ptr_) + pos;
            // It's OK to pass 0 size for memmove according to the C standard.
            ::memmove(p, end, (c_end - end) * sizeof(range_elem));
            // Go to the exact size, because of our very conservative memory
            // strategy.
            d->ptr_ = ::realloc(d->ptr_, new_size * sizeof(range_elem));
            X3ME_ENFORCE(d->ptr_);
            d->size_ = new_size;
            ret      = static_cast<range_elem*>(d->ptr_) + pos;
            break;
        }
        }
        break;
    }
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////

range_vector::const_pointer range_vector::data() const noexcept
{
    return has_data() ? static_cast<range_elem*>(get_data()->ptr_)
                      : get_range_elem();
}

range_vector::size_type range_vector::size() const noexcept
{
    return has_data() ? get_data()->size_ : 1;
}

bool range_vector::empty() const noexcept
{
    return (size() == 0);
}

range_vector::const_iterator range_vector::begin() const noexcept
{
    return data();
}

range_vector::const_iterator range_vector::end() const noexcept
{
    if (has_data())
        return static_cast<range_elem*>(get_data()->ptr_) + get_data()->size_;
    return get_range_elem() + 1;
}

range_vector::const_iterator range_vector::cbegin() const noexcept
{
    return begin();
}

range_vector::const_iterator range_vector::cend() const noexcept
{
    return end();
}

////////////////////////////////////////////////////////////////////////////////

std::pair<range_vector::const_iterator, bool>
range_vector::add_range_impl(const range_elem& rng,
                             size_type cur_capacity) noexcept
{
    std::pair<range_vector::const_iterator, bool> ret(nullptr, false);
    auto* d   = get_data();
    auto* beg = static_cast<const range_elem*>(d->ptr_);
    auto* end = static_cast<const range_elem*>(d->ptr_) + d->size_;
    auto it   = std::lower_bound(beg, end, rng);
    // Don't insert the new range if there are ranges which map
    // fully inside the new one, or if the new one maps fully
    // inside some of the existing ranges.
    if (it == beg)
    {
        // Add the new range at the beginning if it doesn't overlap
        // the current first one.
        if (it->rng_offset() >= rng.rng_end_offset())
        {
            add_at_pos(rng, 0, cur_capacity); // add at position 0
            // Note that the above iterators could be no longer valid
            // at this point because of reallocation.
            ret.first  = static_cast<const range_elem*>(d->ptr_);
            ret.second = true;
        }
        else
        {
            // The current first element overlaps with the new one.
            ret.first  = it;
            ret.second = false;
        }
    }
    else if (it == end)
    {
        // Add the new range at the beginning if it doesn't overlap
        // the current last one.
        --it; // Go to the previous element to check it
        if (rng.rng_offset() >= it->rng_end_offset())
        {
            add_at_pos(rng, d->size_, cur_capacity); // add at the end
            // Note that the above iterators could be no longer valid
            // at this point because of reallocation.
            ret.first  = static_cast<const range_elem*>(d->ptr_) + d->size_ - 1;
            ret.second = true;
        }
        else
        {
            // The current last element overlaps with the new one.
            ret.first  = it;
            ret.second = false;
        }
    }
    else
    {
        // Add the new range at the given position if it doesn't overlap
        // with the previous and the next already existing ranges.
        const auto prev = it - 1;
        const auto next = it;
        if (rng.rng_offset() < prev->rng_end_offset())
        {
            // The previous element overlaps with the new one
            ret.first  = prev;
            ret.second = false;
        }
        else if (next->rng_offset() < rng.rng_end_offset())
        {
            // The next element overlaps with the new one
            ret.first  = next;
            ret.second = false;
        }
        else
        {
            const auto pos = it - beg;
            add_at_pos(rng, pos, cur_capacity);
            // Note that the above iterators could be no longer valid
            // at this point because of reallocation.
            ret.first  = static_cast<const range_elem*>(d->ptr_) + pos;
            ret.second = true;
        }
    }
    return ret;
}

void range_vector::add_at_pos(const range_elem& rng,
                              size_type pos,
                              size_type cur_capacity) noexcept
{
    auto* d = get_data(); // If this function is called we are on the heap
    X3ME_ASSERT(pos <= d->size_, "Wrong argument 'pos'");
    X3ME_ASSERT(d->size_ <= cur_capacity, "Wrong argument 'cur_capacity'");
    if (d->size_ == cur_capacity)
    {
        // We use the most conservative growing strategy because the lowest
        // possible memory consumption is more important to us than the
        // insert speed, because the last happens relatively rarely.
        // This can change in the future.
        d->ptr_ = ::realloc(d->ptr_, (cur_capacity + 1) * sizeof(range_elem));
        X3ME_ENFORCE(d->ptr_);
    }
    auto p = static_cast<range_elem*>(d->ptr_) + pos;
    if (pos < d->size_)
    {
        auto pn = static_cast<range_elem*>(d->ptr_) + pos + 1;
        ::memmove(pn, p, ((d->size_ - pos) * sizeof(range_elem)));
    }
    *p = rng;
    d->size_ += 1;
}

////////////////////////////////////////////////////////////////////////////////

void range_vector::set_empty_data() noexcept
{
    ::memset(data_, 0, sizeof(data_));
    get_data()->magic_ = magic;
}

void range_vector::copy_data(const range_vector& rhs) noexcept
{
    const auto* rhsd = rhs.get_data();
    set_empty_data();
    if (rhsd->size_ > 0)
    {
        auto* d = get_data();
        d->ptr_ = ::malloc(rhsd->size_ * sizeof(range_elem));
        ::memcpy(d->ptr_, rhsd->ptr_, rhsd->size_ * sizeof(range_elem));
        d->size_ = rhsd->size_;
    }
}

void range_vector::move_data(range_vector& rhs) noexcept
{
    ::memcpy(data_, rhs.data_, sizeof(data_));
    rhs.set_empty_data();
}

void range_vector::copy_range_elem(const range_vector& rhs) noexcept
{
    ::memcpy(data_, rhs.data_, sizeof(data_));
    X3ME_ASSERT(!has_data(), "We just copied range_elem. SBO must work");
}

void range_vector::move_range_elem(range_vector& rhs) noexcept
{
    copy_range_elem(rhs);
    rhs.set_empty_data();
}

void range_vector::destroy_data() noexcept
{
    // No destruction of the elements is needed, because they are POD.
    ::free(get_data()->ptr_);
    // No destruction of the data itself is needed because it's POD.
}

////////////////////////////////////////////////////////////////////////////////

bool range_vector::has_data() const noexcept
{
    static_assert(alignof(data_) >= alignof(uint32_t), "");
    static_assert(sizeof(data_) >= sizeof(uint32_t), "");
    const uint32_t* m =
        static_cast<const uint32_t*>(static_cast<const void*>(data_));
    return (*m == magic);
}

range_vector::container_data* range_vector::get_data() noexcept
{
    return static_cast<container_data*>(static_cast<void*>(data_));
}

const range_vector::container_data* range_vector::get_data() const noexcept
{
    return static_cast<const container_data*>(static_cast<const void*>(data_));
}

range_elem* range_vector::get_range_elem() noexcept
{
    return static_cast<range_elem*>(static_cast<void*>(data_));
}

const range_elem* range_vector::get_range_elem() const noexcept
{
    return static_cast<const range_elem*>(static_cast<const void*>(data_));
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os,
                         const range_vector::iter_range& rhs) noexcept
{
    os << rhs.size() << '[';
    for (const auto& r : rhs)
        os << r << ',';
    os << ']';
    return os;
}

std::ostream& operator<<(std::ostream& os, const range_vector& rhs) noexcept
{
    os << rhs.size() << '[';
    for (const auto& r : rhs)
        os << r << ',';
    os << ']';
    return os;
}

} // namespace detail
} // namespace cache
