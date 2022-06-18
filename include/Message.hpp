#ifndef ROBUST_FILE_TRANSFER_MESSAGE_HPP
#define ROBUST_FILE_TRANSFER_MESSAGE_HPP
// ------------------------------------------------------------------------
#include "common.hpp"
// ------------------------------------------------------------------------
namespace rft
{
   // ------------------------------------------------------------------------
   template<typename MsgType>
   struct MessageHeader {
      MsgType id;
      uint16_t size;
   };
   // ------------------------------------------------------------------------
   template<typename MsgType>
   struct Message {
      MessageHeader<MsgType> header;
      char body[rft::PACKET_SIZE];
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_MESSAGE_HPP
// ------------------------------------------------------------------------
