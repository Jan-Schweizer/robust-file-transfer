// ------------------------------------------------------------------------
#include "Client.hpp"
#include <boost/bind/bind.hpp>
#include <plog/Log.h>
// ------------------------------------------------------------------------
namespace rft
{
   using namespace boost::asio;
   // ------------------------------------------------------------------------
   Client::Client(boost::asio::io_context& io_context, std::string host, const size_t port)
       : socket(io_context, ip::udp::endpoint(ip::udp::v4(), port + 1)), host(std::move(host)), port(port)
   {
      bool server_resolved = resolve_server();
      if (!server_resolved) {
         return;
      }

      create_echo_msg();

      send_msg(tmpMsgOut);
   }
   // ------------------------------------------------------------------------
   Client::~Client() { stop(); }
   // ------------------------------------------------------------------------
   bool Client::resolve_server()
   {
      try {
         ip::udp::resolver resolver(io_context);
         server_endpoint = *resolver.resolve(ip::udp::v4(), host, std::to_string(port)).begin();

         PLOG_INFO << "[Client] Resolved server at " + server_endpoint.address().to_string() + ":" + std::to_string(server_endpoint.port());
      } catch (std::exception& e) {
         PLOG_ERROR << "[Client] Error while trying to resolve_server to server";
         return false;
      }
      return true;
   }
   // ------------------------------------------------------------------------
   void Client::stop()
   {
      io_context.stop();
      PLOG_INFO << "[Client] Disconnected!";
   }
   // ------------------------------------------------------------------------
   void Client::send_msg(Message<ClientMsgType>& msg)
   {
      socket.async_send_to(buffer(msg.body, PACKET_SIZE), server_endpoint,
                           boost::bind(&Client::handle_send, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));

      recv_msg();
   }
   // ------------------------------------------------------------------------
   void Client::handle_send(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Client] Send message";
         PLOG_INFO << "\"" + std::string(&tmpMsgOut.body[3]) + "\"";
      } else {
         PLOG_WARNING << "[Client] Error on Send: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Client::recv_msg()
   {
      socket.async_receive_from(buffer(tmpMsgIn.body, PACKET_SIZE), remote_endpoint,
                                boost::bind(&Client::handle_receive, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Client::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Client] Received message";
         PLOG_INFO << "\"" + std::string(&tmpMsgIn.body[3]) + "\"";
         decode_msg(tmpMsgIn.body);
      } else {
         PLOG_WARNING << "[Client] Error on Receive: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Client::decode_msg(char msg[PACKET_SIZE])
   {
      ConnectionID connectionId = *reinterpret_cast<ConnectionID*>(&msg[0]);
      uint8_t msgType = msg[2] >> 4;
      uint8_t version = msg[2] & 0b00001111;
      std::string payload(&msg[3]);
      PLOG_INFO << "ConnectionID: " << connectionId << " - msgType: " << +msgType << " - version: " << +version << " - payload: " << payload;
   }
   // ------------------------------------------------------------------------
   void Client::create_echo_msg()
   {
      // ConnectionID (not assigned yet)
      tmpMsgOut.body[0] = 0;
      tmpMsgOut.body[1] = 0;
      // MsgType
      tmpMsgOut.body[2] = ClientMsgType::FILE_REQUEST << 4;
      // Version
      tmpMsgOut.body[2] = tmpMsgOut.body[2] | 0b0001;
      // Payload
      std::string payload("Hello from client!");
      std::memcpy(&tmpMsgOut.body[3], payload.data(), payload.size() + 1);
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
