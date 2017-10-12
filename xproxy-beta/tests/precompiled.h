#ifndef PRECOMPILED_H
#define PRECOMPILED_H

////////////////////////////////////////////////////////////////////////////////
// system headers
#include <assert.h>
#include <linux/fs.h>
#include <pcre.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/raw.h>
#include <sys/statvfs.h>
#include <zlib.h>

////////////////////////////////////////////////////////////////////////////////
// C++ std headers
#include <algorithm>
#include <array>
#include <condition_variable>
#include <deque>
#include <experimental/optional>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>

////////////////////////////////////////////////////////////////////////////////
// boost headers
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/string.hpp>
#include <boost/crc.hpp>
#include <boost/expected/expected.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/sml.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/utility/string_view.hpp>

////////////////////////////////////////////////////////////////////////////////
// Other 3rd party library headers
#include <fmt/ostream.h>
#include <http-parser/http_parser.h>
#include <openssl/md5.h>
#include <sparsehash/sparse_hash_map>

////////////////////////////////////////////////////////////////////////////////
// x3me_libs headers
#include <x3me_libs/const_string.h>
#include <x3me_libs/math_funcs.h>
#include <x3me_libs/mem_fn_delegate.h>
#include <x3me_libs/pimpl.h>
#include <x3me_libs/print_utils.h>
#include <x3me_libs/scope_guard.h>
#include <x3me_libs/shared_mutex.h>
#include <x3me_libs/spin_lock.h>
#include <x3me_libs/stack_string.h>
#include <x3me_libs/string_builder.h>
#include <x3me_libs/synchronized.h>
#include <x3me_libs/sys_utils.h>
#include <x3me_libs/utils.h>
#include <x3me_libs/array_view.h>

////////////////////////////////////////////////////////////////////////////////
// this project headers
#include "common_funcs.h"
#include "common_types.h"
#include "../logging.h"

#endif // PRECOMPILED_H
