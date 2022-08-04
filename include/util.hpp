#ifndef ROBUST_FILE_TRANSFER_UTIL_HPP
#define ROBUST_FILE_TRANSFER_UTIL_HPP
// ------------------------------------------------------------------------
#include "common.hpp"
#include <string>
// ------------------------------------------------------------------------
namespace rft
{
   void compute_file_SHA256(std::string& filename, unsigned char ret[SHA256_SIZE]);
   void compute_SHA256(unsigned char* buffer, size_t size, unsigned char ret[SHA256_SIZE]);

   // https://gist.github.com/ccbrown/9722406
   void hexdump(const void* data, size_t size);
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_UTIL_HPP
