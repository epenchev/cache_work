#include "precompiled.h"
#include "http_trans.h"
#include "http_msg_parser.ipp"

// The only purpose of this .cpp is to instantiate the parser.
// It can be instantiated implicitly by the http_trans.cpp, but IMO
// this way will speed the compilation of the http_trans.cpp when changes
// are done there.
template class http::http_msg_parser<http::http_trans, http::req_parser>;
template class http::http_msg_parser<http::http_trans, http::resp_parser>;
