#ifndef ROBUST_FILE_TRANSFER_COMMON_HPP
#define ROBUST_FILE_TRANSFER_COMMON_HPP
// ------------------------------------------------------------------------
#include <boost/asio.hpp>
#include <cstdint>
#include "plog/Log.h"
// ------------------------------------------------------------------------
namespace rft
{
   using namespace boost::asio;

   using ConnectionID = uint16_t;

   const uint16_t PACKET_SIZE = 512;
   const uint8_t SHA256_SIZE = 64;
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_COMMON_HPP
// ------------------------------------------------------------------------
