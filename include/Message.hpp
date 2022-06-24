#ifndef ROBUST_FILE_TRANSFER_MESSAGE_HPP
#define ROBUST_FILE_TRANSFER_MESSAGE_HPP
// ------------------------------------------------------------------------
#include "common.hpp"
// ------------------------------------------------------------------------
namespace rft
{
   // ------------------------------------------------------------------------
   enum ClientMsgType : uint8_t
   {
      // Standard Types
      FILE_REQUEST = 0,

      // Error Types
      RETRANSMISSION_REQUEST = 8
   };
   // ------------------------------------------------------------------------
   enum ServerMsgType : uint8_t
   {
      // Standard Types
      PAYLOAD = 0,

      // Error Types
      FILE_NOT_FOUND = 8
   };
   // ------------------------------------------------------------------------
   template<typename MsgType>
   struct MessageHeader {
      MsgType type;
      uint16_t size;
   };
   // ------------------------------------------------------------------------
   template<typename MsgType>
   struct Message {
      MessageHeader<MsgType> header;
      char body[PACKET_SIZE]{'\0'};
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_MESSAGE_HPP
// ------------------------------------------------------------------------
