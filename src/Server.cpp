// ------------------------------------------------------------------------
#include "Server.hpp"
#include <boost/bind/bind.hpp>
#include <plog/Log.h>
// ------------------------------------------------------------------------
namespace rft
{
   using namespace boost::asio;
   // ------------------------------------------------------------------------
   Server::Server(const size_t port)
       : socket(io_context, ip::udp::endpoint(ip::udp::v4(), port)), port(port) {}
   // ------------------------------------------------------------------------
   Server::~Server() { stop(); }
   // ------------------------------------------------------------------------
   void Server::start()
   {
      receive_msg();
      thread_context = std::thread([this]() { io_context.run(); });
      PLOG_INFO << "[Server] Started on port " + std::to_string(port) + "!";

      process_msgs();
   }
   // ------------------------------------------------------------------------
   void Server::stop()
   {
      if (thread_context.joinable()) thread_context.join();
      io_context.stop();
      PLOG_INFO << "[Server] Stopped!";
   }
   // ------------------------------------------------------------------------
   void Server::receive_msg()
   {
      socket.async_receive_from(
          buffer(tmpMsgIn.body, PACKET_SIZE), remote_endpoint,
          boost::bind(&Server::handle_receive, this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Server::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Server] Received message";
         PLOG_INFO << "\"" + std::string(&tmpMsgIn.body[3]) + "\"";
         enqueue_msg(bytes_transferred);
      } else {
         PLOG_WARNING << "[Server] Error on Receive: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Server::send_msg_to_client(Message<ServerMsgType>& msg, const ip::udp::endpoint& client)
   {
      // change remote_endpoint to parameter client
      socket.async_send_to(buffer(msg.body, PACKET_SIZE), client,
                           boost::bind(&Server::handle_send, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Server::handle_send(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Server] Send message";
         PLOG_INFO << "\"" + std::string(&tmpMsgOut.body[3]) + "\"";
      } else {
         PLOG_WARNING << "[Server] Error on Send: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Server::decode_msg(size_t bytes_transferred)
   {
      auto& msg = tmpMsgIn.body;

      ConnectionID connectionId = *reinterpret_cast<ConnectionID*>(&msg[0]);
      uint8_t msgType = msg[2] >> 4;
      uint8_t version = msg[2] & 0b00001111;
      std::string payload(&msg[3]);

      tmpMsgIn.header.type = static_cast<ClientMsgType>(msgType);
      tmpMsgIn.header.size = bytes_transferred - sizeof(ConnectionID) - 1;

      PLOG_INFO << "ConnectionID: " << connectionId << " - msgType: " << +msgType << " - version: " << +version << " - payload: " << payload;
   }
   // ------------------------------------------------------------------------
   void Server::enqueue_msg(size_t bytes_transferred)
   {
      decode_msg(bytes_transferred);
      msgQueue.push_back(tmpMsgIn);

      receive_msg();
   }
   // ------------------------------------------------------------------------
   void Server::process_msgs()
   {
      while (true) {
         msgQueue.wait();

         size_t msgCount = 0;
         while (!msgQueue.empty()) {
            auto msg = msgQueue.pop_front();
            echo_msg(msg);
            ++msgCount;
         }
      }
   }
   // ------------------------------------------------------------------------
   void Server::echo_msg(Message<ClientMsgType>& msg)
   {
      std::string response("Echoing: " + std::string(&msg.body[3]));

      tmpMsgOut.header.type = ServerMsgType::PAYLOAD;
      tmpMsgOut.header.size = response.size() + 1;

      ++connectionIdPool;
      std::memcpy(&tmpMsgOut.body[0], &connectionIdPool, sizeof(uint16_t));
      tmpMsgOut.body[2] = ServerMsgType::PAYLOAD << 4;
      tmpMsgOut.body[2] = tmpMsgOut.body[2] | 0b0001;
      std::memcpy(&tmpMsgOut.body[3], response.data(), response.size() + 1);

      send_msg_to_client(tmpMsgOut, remote_endpoint);
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
