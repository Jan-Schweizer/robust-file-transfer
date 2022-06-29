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
      FILE_REQUEST = 0,// aka Client Initial Request
      TRANSMISSION_REQUEST = 1,
      RETRANSMISSION_REQUEST = 2,

      // Error Types
      CLIENT_ERROR = 8
   };
   // ------------------------------------------------------------------------
   enum ServerMsgType : uint8_t
   {
      // Standard Types
      SERVER_INITIAL_RESPONSE = 0,
      PAYLOAD = 1,// aka Server Data Response

      // Error Types
      SERVER_ERROR = 8
   };
   // ------------------------------------------------------------------------
   template<typename MsgType>
   struct MessageHeader {
      MsgType type;
      uint16_t size = 0;
      boost::asio::ip::udp::endpoint remote;
   };
   // ------------------------------------------------------------------------
   template<typename MsgType>
   struct Message {
      MessageHeader<MsgType> header;
      char body[PACKET_SIZE]{'\0'};

      /// Pushes T (stack-like) into the message
      template<typename T>
      friend Message<MsgType>& operator<<(Message<MsgType>& msg, const std::pair<const T&, size_t> data)
      {
         std::memcpy(&msg.body[msg.header.size], &data.first, data.second);
         msg.header.size += data.second;
         return msg;
      }

      /// Pops data (stack-like) from the message into T
      template<typename T>
      friend Message<MsgType>& operator>>(Message<MsgType>& msg, const std::pair<T&, size_t> data)
      {
         msg.header.size -= data.second;
         std::memcpy(&data.first, &msg.body[msg.header.size], data.second);
         return msg;
      }
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_MESSAGE_HPP
// ------------------------------------------------------------------------
