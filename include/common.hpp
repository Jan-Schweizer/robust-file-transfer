#ifndef ROBUST_FILE_TRANSFER_COMMON_HPP
#define ROBUST_FILE_TRANSFER_COMMON_HPP
// ------------------------------------------------------------------------
#include "plog/Log.h"
#include <boost/asio.hpp>
#include <cstdint>
// ------------------------------------------------------------------------
namespace rft
{
   using namespace boost::asio;

   using ConnectionID = uint16_t;

   const uint16_t PACKET_SIZE = 512;
   const uint8_t SHA256_SIZE = 32;
   const uint8_t MAX_WINDOW_SIZE = 64;
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_COMMON_HPP
// ------------------------------------------------------------------------
