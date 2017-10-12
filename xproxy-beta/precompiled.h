#ifndef PRECOMPILED_H
#define PRECOMPILED_H

////////////////////////////////////////////////////////////////////////////////
// system headers
#include <grp.h>
#include <linux/fs.h>
#include <linux/netlink.h>
#include <pcre.h>
#include <pwd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/raw.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

////////////////////////////////////////////////////////////////////////////////
// C++ std headers
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <experimental/optional>
#include <experimental/tuple>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <numeric>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>

////////////////////////////////////////////////////////////////////////////////
// boost headers
#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/container/string.hpp>
#include <boost/crc.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/expected/expected.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/make_shared.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/sml.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/thread/latch.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/get.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/variant.hpp>

////////////////////////////////////////////////////////////////////////////////
// Other 3rd party library headers
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <fmt/ostream.h>
#include <http-parser/http_parser.h>
#include <openssl/md5.h>
#include <sparsehash/sparse_hash_map>

////////////////////////////////////////////////////////////////////////////////
// x3me_libs headers
#include <x3me_libs/array_view.h>
#include <x3me_libs/build_utils.h>
#include <x3me_libs/const_string.h>
#include <x3me_libs/inplace_fn.h>
#include <x3me_libs/json_rpc/json_rpc_server.h>
#include <x3me_libs/math_funcs.h>
#include <x3me_libs/mem_fn_delegate.h>
#include <x3me_libs/pimpl.h>
#include <x3me_libs/print_utils.h>
#include <x3me_libs/rcu_resource.h>
#include <x3me_libs/mem_fn_delegate.h>
#include <x3me_libs/pimpl.h>
#include <x3me_libs/shared_mutex.h>
#include <x3me_libs/scope_guard.h>
#include <x3me_libs/shared_mutex.h>
#include <x3me_libs/spin_lock.h>
#include <x3me_libs/stack_string.h>
#include <x3me_libs/stacktrace.h>
#include <x3me_libs/string_builder.h>
#include <x3me_libs/synchronized.h>
#include <x3me_libs/sys_utils.h>
#include <x3me_libs/tagged_ptr.h>
#include <x3me_libs/utils.h>
#include <x3me_libs/x3me_assert.h>

////////////////////////////////////////////////////////////////////////////////
// this project headers
#include "common_types.h"
#include "common_funcs.h"
#include "logging.h"

#endif // PRECOMPILED_H
