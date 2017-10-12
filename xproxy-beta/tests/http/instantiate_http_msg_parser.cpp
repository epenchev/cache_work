#include "precompiled.h"
#include "parser_notified.h"
#include "../http/http_trans.h"
#include "../http/http_msg_parser.h"
#include "../http/http_msg_parser.ipp"

// Avoid parsing of the *.ipp file every time when some of the other .cpp files
// using the parser is changed.
template class http::http_msg_parser<req_parser_notified, http::req_parser>;
template class http::http_msg_parser<resp_parser_notified, http::resp_parser>;

template class http::http_msg_parser<http::http_trans, http::req_parser>;
template class http::http_msg_parser<http::http_trans, http::resp_parser>;
