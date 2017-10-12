#pragma once

namespace cache
{
namespace detail
{

enum : uint64_t
{
    // We write to disk in store blocks.
    store_block_size = 4_KB,
#if defined(X3ME_TEST) || defined(X3ME_APP_TEST)
    volume_block_size = store_block_size,
#else
    volume_block_size = 512, // Bytes
#endif // X3ME_TEST
    // This offset is used because we want to be able to distinguish between
    // the raw disks and already partitioned (fdisk) disks.
    // When a disk is partitioned with fdisk the default and minimum allowed
    // offset for the partition is 2048 sectors. The sector size in 99.99% of
    // the cases is 512 bytes thus the offset of 2048 * 512 = 1_MB.
    volume_skip_bytes = 1_MB,

    agg_write_meta_size  = 4_KB,
    agg_write_data_size  = 4_MB,
    agg_write_block_size = agg_write_meta_size + agg_write_data_size,

    // We need to store as little as 1 byte if we want to be sure that
    // we can collect full objects.
    object_frag_min_data_size = 1,
    object_frag_max_data_size = 1_MB,
    object_frag_hdr_size      = 8,
    object_frag_max_size      = object_frag_hdr_size + object_frag_max_data_size,

    metadata_sync_size = 4_MB,

    min_volume_size = 32_MB,
    max_volume_size = 512_TB,

    // Min and max supported objects
    min_obj_size = 8_KB,
    max_obj_size = 8_GB,
};
static_assert((volume_skip_bytes % store_block_size) == 0,
              "The start offset needs to be multiple of the store_block_size");
static_assert(volume_skip_bytes < min_volume_size,
              "Can't skip the whole volume");
static_assert(object_frag_max_size <= agg_write_data_size,
              "An object fragment must not go to multiple aggregation writes");
static_assert((agg_write_block_size % store_block_size) == 0,
              "We need to write in multiple of the store block size");

template <typename NumType>
constexpr NumType round_to_volume_block_size(NumType num) noexcept
{
    static_assert(x3me::math::is_pow_of_2(volume_block_size), "");
    return x3me::math::round_up_pow2(num, volume_block_size);
}

template <typename NumType>
constexpr NumType round_to_store_block_size(NumType num) noexcept
{
    static_assert(x3me::math::is_pow_of_2(store_block_size), "");
    return x3me::math::round_up_pow2(num, store_block_size);
}

////////////////////////////////////////////////////////////////////////////////

} // namespace detail
} // namespace cache
