#ifndef ROBUST_FILE_TRANSFER_COMMON_HPP
#define ROBUST_FILE_TRANSFER_COMMON_HPP
// ------------------------------------------------------------------------
#include "plog/Log.h"
#include <boost/asio.hpp>
#include <cstdint>
// ------------------------------------------------------------------------
#define NOW boost::asio::chrono::high_resolution_clock::now()
// ------------------------------------------------------------------------
namespace rft
{
   using namespace boost::asio;

   using ConnectionID = uint16_t;

   using timepoint = chrono::time_point<boost::asio::chrono::high_resolution_clock>;
   using nanos = chrono::nanoseconds;
   using micros = chrono::microseconds;
   using millis = chrono::milliseconds;
   using timeunit = micros;

   /// Size of a data chunk from the file
   const uint16_t CHUNK_SIZE = 512;
   /// Size of the Server Payload Packet meta data
   const uint16_t PAYLOAD_META_DATA_SIZE = sizeof(uint8_t) + sizeof(ConnectionID) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t);
   /// Maximum size of a packet (Server Payload Packet aka Server Data Response)
   const uint16_t MAX_PACKET_SIZE = CHUNK_SIZE + PAYLOAD_META_DATA_SIZE;
   /// Size of the SHA256 hash
   const uint8_t SHA256_SIZE = 32;
   /// Maximum receiving chunk size of a client
   const uint16_t MAX_WINDOW_SIZE = 64;
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_COMMON_HPP
// ------------------------------------------------------------------------
