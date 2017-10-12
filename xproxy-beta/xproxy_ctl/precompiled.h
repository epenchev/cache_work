#ifndef PRECOMPILED_H
#define PRECOMPILED_H

////////////////////////////////////////////////////////////////////////////////
// system headers

////////////////////////////////////////////////////////////////////////////////
// C++ std headers
#include <iostream>
#include <numeric>
#include <string>
#include <type_traits>
#include <vector>

////////////////////////////////////////////////////////////////////////////////
// boost headers
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>
#include <boost/utility/string_view.hpp>

////////////////////////////////////////////////////////////////////////////////
// Other 3rd party library headers
#define URDL_DISABLE_SSL 1
#define URDL_HEADER_ONLY 1
#include <urdl/http.hpp>
#include <urdl/istream.hpp>
#undef URDL_DISABLE_SSL
#undef URDL_HEADER_ONLY
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>

////////////////////////////////////////////////////////////////////////////////
// x3me_libs headers
#include <x3me_libs/build_utils.h>
#include <x3me_libs/stacktrace.h>
#include <x3me_libs/string_builder.h>
#include <x3me_libs/utils.h>

////////////////////////////////////////////////////////////////////////////////
// tsis project headers
#include "common.h"
#include "debug_log.h"

#endif // PRECOMPILED_H
