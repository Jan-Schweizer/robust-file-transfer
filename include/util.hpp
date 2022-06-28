#ifndef ROBUST_FILE_TRANSFER_UTIL_HPP
#define ROBUST_FILE_TRANSFER_UTIL_HPP
// ------------------------------------------------------------------------
#include <string>
// ------------------------------------------------------------------------
namespace rft
{
   std::string compute_SHA256(std::string& filename);

   // https://gist.github.com/ccbrown/9722406
   void hexdump(const void* data, size_t size);
}
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_UTIL_HPP
