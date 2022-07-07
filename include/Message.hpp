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
      unsigned char packet[MAX_PACKET_SIZE]{'\0'};

      /// Pushes T (stack-like) into the message
      template<typename T>
      friend Message<MsgType>& operator<<(Message<MsgType>& msg, const T& data)
      {
         std::memcpy(&msg.packet[msg.header.size], &data, sizeof(data));
         msg.header.size += sizeof(data);
         return msg;
      }

      // Special overload for std::string
      friend Message<MsgType>& operator<<(Message<MsgType>& msg, const std::string& data)
      {
         std::memcpy(&msg.packet[msg.header.size], data.data(), data.size() + 1);
         msg.header.size += data.size() + 1;
         return msg;
      }

      // Special overload for std::vector<unsigned char>
      friend Message<MsgType>& operator<<(Message<MsgType>& msg, const std::vector<unsigned char>& chunk)
      {
         std::memcpy(&msg.packet[msg.header.size], chunk.data(), chunk.size());
         msg.header.size += chunk.size();
         return msg;
      }
      // ------------------------------------------------------------------------
      /// Pops data (stack-like) from the message into T
      template<typename T>
      friend Message<MsgType>& operator>>(Message<MsgType>& msg, T& data)
      {
         msg.header.size -= sizeof(data);
         std::memcpy(&data, &msg.packet[msg.header.size], sizeof(data));
         return msg;
      }

      // Special overload for std::string
      friend Message<MsgType>& operator>>(Message<MsgType>& msg, std::string& data)
      {
         msg.header.size -= data.size() + 1;
         std::memcpy(data.data(), &msg.packet[msg.header.size], data.size() + 1);
         return msg;
      }

      // Special overload for std::vector<unsigned char>
      friend Message<MsgType>& operator>>(Message<MsgType>& msg, std::vector<unsigned char>& chunk)
      {
         msg.header.size -= chunk.size();
         std::memcpy(chunk.data(), &msg.packet[msg.header.size], chunk.size());
         return msg;
      }
   };
}// namespace rft
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_MESSAGE_HPP
// ------------------------------------------------------------------------
