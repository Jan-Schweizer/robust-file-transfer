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
   using seconds = chrono::seconds;
   using minutes = chrono::minutes;
   using timeunit = micros;

   /// Size of a data chunk from the file
   const uint16_t CHUNK_SIZE = 512;
   /// Size of the SHA256 hash
   const uint8_t SHA256_SIZE = 32;
   /// Maximum throughput a client can handle in MB/s
   const uint16_t MAX_THROUGHPUT = 1;
   /// Size of the Server Validation Request meta data (without filename hence)
   const uint16_t SERVER_VALIDATION_REQUEST_META_DATA_SIZE = sizeof(uint8_t) + sizeof(uint8_t) + SHA256_SIZE + SHA256_SIZE + sizeof(uint32_t) + 1;
   /// Size of the Client Validation Response meta data (without filename hence)
   const uint16_t CLIENT_VALIDATION_RESPONSE_META_DATA_SIZE = sizeof(uint8_t) + SHA256_SIZE + sizeof(uint32_t) + sizeof(uint16_t) + 1;
   /// Size of the Server Initial Response meta data (without filename)
   const uint16_t SERVER_INITIAL_RESPONSE_META_DATA = sizeof(uint8_t) + sizeof(ConnectionID) + sizeof(uint64_t) + SHA256_SIZE + 1;
   /// Size of the Client Validation failed meta data (without filename)
   const uint16_t CLIENT_VALIDATION_FAILED_META_DATA = sizeof(uint8_t) + 1;
   /// Size of the File Not Found meta data (without filename)
   const uint16_t FILE_NOT_FOUND_META_DATA = sizeof(uint8_t) + 1;
   /// Size of the Server Payload Packet meta data
   const uint16_t PAYLOAD_META_DATA_SIZE = sizeof(uint8_t) + sizeof(ConnectionID) + sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint16_t);
   /// Maximum size of a packet (Server Payload Packet aka Server Data Response)
   const uint16_t MAX_PACKET_SIZE = CHUNK_SIZE + PAYLOAD_META_DATA_SIZE;
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_COMMON_HPP
// ------------------------------------------------------------------------
