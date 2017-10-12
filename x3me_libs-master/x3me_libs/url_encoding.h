#pragma once

namespace x3me
{
namespace urlencoding
{

x3me::types::string_t url_encode(const x3me::types::string_t& s);
x3me::types::string_t url_decode(const x3me::types::string_t& s);
void url_decode_inplace(x3me::types::string_t& s);

} // namespace urlencoding
} // namespace x3me

