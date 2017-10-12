#pragma once

#include "range_elem.h"
#include "range.h"

namespace cache
{
namespace detail
{

class range_vector
{
public:
    using value_type      = range_elem;
    using pointer         = range_elem*;
    using const_pointer   = const range_elem*;
    using const_reference = const range_elem&;
    using const_iterator  = const range_elem*;
    using size_type       = uint32_t;
    using iter_range      = boost::iterator_range<const_iterator>;

private:
    static constexpr uint32_t max_ranges = 8193;
    // Increase the 'max_ranges' to the printed number, if this asserts
    X3ME_STATIC_NUM_CHECK(
        (uint64_t(max_ranges) * range_elem::max_rng_size()) >= max_obj_size,
        x3me::math::divide_round_up(max_obj_size, range_elem::max_rng_size()));

private:
    static constexpr uint32_t magic = 0xFEEDCAFE;
    static_assert(uint8_t(magic) != range_elem::elem_mark,
                  "Need to distinguish between range_elem and the container "
                  "data due to the SBO");
    struct container_data
    {
        uint32_t magic_; // To distinguish between container_data and range_elem
        uint32_t size_;
        void* ptr_;
    };
    static_assert(std::is_pod<container_data>::value,
                  "Needed for memcpy, memset, etc");
    static_assert(sizeof(container_data) == sizeof(range_elem),
                  "Needed for SBO");
    static_assert(alignof(container_data) >= alignof(range_elem),
                  "Needed for SBO");

    alignas(container_data) uint8_t data_[sizeof(container_data)];

public:
    // Change the value returned by this function if the class stops to
    // use Small Buffer Optimization. Some of the components of the above
    // layer may rely on this.
    static constexpr bool has_sbo() noexcept { return true; }

public:
    range_vector() noexcept;
    explicit range_vector(const range_elem& rhs) noexcept;
    ~range_vector() noexcept;

    range_vector(const range_vector& rhs) noexcept;
    range_vector& operator=(const range_vector& rhs) noexcept;
    range_vector(range_vector&& rhs) noexcept;
    range_vector& operator=(range_vector&& rhs) noexcept;

    // Won't add the range if it overlaps with some of the existing ranges.
    std::pair<const_iterator, bool> add_range(const range_elem& rng) noexcept;

    // Returns iterators to the range elements which are included in the
    // given range. Partially overlapped range elements are also included.
    // A pair of end_iterators is returned in case of no elements found or
    // if there are holes between the range elements found in this range.
    // Note that the returned range is equal or bigger to the requested one.
    iter_range find_full_range(range rng) const noexcept;

    // Returns iterators to the range elements which are included in the
    // given range, only if these range elements form the exact range.
    // A pair of end_iterators is returned in case of no elements found or
    // if there are holes between the range elements found in this range,
    // or if the found continuous range is not exactly the same as the
    // requested one.
    iter_range find_exact_range(range rng) const noexcept;

    const_iterator find_exact_range(const range_elem& rng) const noexcept;

    // Returns iterators to the first element which is found to overlap with
    // the given range and to the last such element. No matter if there are
    // holes between the elements or not. A pair of end_iterators is returned
    // in case of no elements found.
    iter_range find_in_range(range rng) const noexcept;

    // TODO Remove this function if not used anymore.
    static bool are_continuous(iter_range rngs) noexcept;

    // Removes the overlapping at the beginning and at the end with already
    // existing ranges i.e. trims the overlaps from the beginning and from the
    // end. It doesn't remove overlaps in the middle.
    // If we have a continuous ranges which overlap the whole passed range,
    // the function returns empty range.
    // The function returns empty range also if the resulting range size after
    // the trimming becomes less than the minimum allowed object size.
    range trim_overlaps(range rng) const noexcept;

    // Removing a range invalidates all current iterators.
    // Returns iterator to the next element or the end iterator.
    const_iterator rem_range(iter_range rng) noexcept;
    const_iterator rem_range(const_iterator it) noexcept
    {
        return rem_range(iter_range(it, it + 1));
    }

    const_pointer data() const noexcept;
    size_type size() const noexcept;
    bool empty() const noexcept;

    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    // Returns true if the range_vector is successfully loaded.
    // Returns false if the range_vector is invalid and can't be loaded.
    // Throws in case of IO error.
    // If the function throws the range_vector remains in unspecified state,
    // and the only safe things are either to destroy it, or to copy/move
    // another one to it, which eventually destroys it too.
    template <typename Reader>
    bool load(Reader& reader);

    template <typename Writer>
    void save(Writer& writer) const noexcept;

private:
    std::pair<const_iterator, bool>
    add_range_impl(const range_elem& rng, size_type cur_capacity) noexcept;
    void add_at_pos(const range_elem& rng, size_type pos,
                    size_type cur_capacity) noexcept;

    void set_empty_data() noexcept;
    void copy_data(const range_vector& rhs) noexcept;
    void move_data(range_vector& rhs) noexcept;
    void copy_range_elem(const range_vector& rhs) noexcept;
    void move_range_elem(range_vector& rhs) noexcept;
    void destroy_data() noexcept;

    bool has_data() const noexcept;
    container_data* get_data() noexcept;
    const container_data* get_data() const noexcept;
    range_elem* get_range_elem() noexcept;
    const range_elem* get_range_elem() const noexcept;
};

////////////////////////////////////////////////////////////////////////////////
// The range vector keeps its elements sorted and we don't want to provide
// non-const access to all parts of the range element. Thus this hacky way
// is used. This can be made more safer if the range_vector::const_iterator
// is not just an alias for a pointer to range_elem.
inline void rv_elem_set_disk_offset(range_vector::const_iterator e,
                                    volume_blocks64_t v) noexcept
{
    const_cast<range_elem*>(e)->set_disk_offset(v);
}

inline void rv_elem_set_in_memory(range_vector::const_iterator e,
                                  bool v) noexcept
{
    const_cast<range_elem*>(e)->set_in_memory(v);
}

inline bool rv_elem_atomic_inc_readers(range_vector::const_iterator e) noexcept
{
    return const_cast<range_elem*>(e)->atomic_inc_readers();
}

inline void rv_elem_atomic_dec_readers(range_vector::const_iterator e) noexcept
{
    const_cast<range_elem*>(e)->atomic_dec_readers();
}

inline void rv_elem_reset_meta(range_vector::const_iterator e) noexcept
{
    const_cast<range_elem*>(e)->reset_meta();
}

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os,
                         const range_vector::iter_range& rhs) noexcept;
std::ostream& operator<<(std::ostream& os, const range_vector& rhs) noexcept;

////////////////////////////////////////////////////////////////////////////////

template <typename Reader>
bool range_vector::load(Reader& reader)
{
    X3ME_ASSERT(empty(),
                "The method is intended to be used for initialization");
    reader.read(data_, sizeof(data_));
    if (has_data())
    {
        auto* d = get_data();
        if ((d->size_ > 1) && (d->size_ <= max_ranges))
        {
            const auto sz = d->size_ * sizeof(range_elem);
            d->ptr_ = ::malloc(sz);
            X3ME_ENFORCE(d->ptr_);
            // Although we may throw in this call, nothing so bad can happen,
            // because the range_elem is POD data and don't need to be
            // destroyed. So just freeing the memory upon destruction is OK.
            reader.read(d->ptr_, sz);
        }
        else
        {
            set_empty_data(); // Pretend that nothing has happened
            return false;
        }
    }
    else if (!range_elem::is_range_elem(data_))
    {
        set_empty_data(); // Pretend that nothing has happened
        return false;
    }
    return true;
}

template <typename Writer>
void range_vector::save(Writer& writer) const noexcept
{
    writer.write(data_, sizeof(data_));
    if (has_data())
    {
        auto* d = get_data();
        writer.write(d->ptr_, d->size_ * sizeof(range_elem));
    }
}

} // namespace detail
} // namespace cache
