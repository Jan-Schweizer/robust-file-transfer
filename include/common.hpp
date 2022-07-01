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

   /// Size of a data chunk from the file
   const uint16_t CHUNK_SIZE = 512;
   /// Maximum size of a packet (Server Payload Packet aka Server Data Response)
   const uint16_t MAX_PACKET_SIZE = CHUNK_SIZE + sizeof(uint8_t) + sizeof(ConnectionID) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t);
   /// Size of the SHA256 hash
   const uint8_t SHA256_SIZE = 32;
   /// Maximum receiving window size of a client
   const uint16_t MAX_WINDOW_SIZE = 64;
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_COMMON_HPP
// ------------------------------------------------------------------------
