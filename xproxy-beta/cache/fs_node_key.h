#pragma once

#include "xutils/md5_hash.h"

namespace cache
{
namespace detail
{

// TODO Make it class if we are going to have different hash keys.
// This will give us type-safety.
using fs_node_key_t = xutils::md5_hash;

} // namespace detail
} // namespace cache
