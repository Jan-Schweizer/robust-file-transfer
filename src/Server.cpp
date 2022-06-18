// ------------------------------------------------------------------------
#include "Server.hpp"
#include <boost/bind/bind.hpp>
#include <plog/Log.h>
// ------------------------------------------------------------------------
namespace rft
{
   using namespace boost::asio;
   // ------------------------------------------------------------------------
   Server::Server(boost::asio::io_context& io_context, const size_t port)
       : socket(io_context, ip::udp::endpoint(ip::udp::v4(), port)), port(port)
   {
      start();
   }
   // ------------------------------------------------------------------------
   Server::~Server() { stop(); }
   // ------------------------------------------------------------------------
   void Server::start()
   {
      receive_msg();
      PLOG_INFO << "[Server] Started on port " + std::to_string(port) + "!";
   }
   // ------------------------------------------------------------------------
   void Server::stop()
   {
      io_context.stop();
      PLOG_INFO << "[Server] Stopped!";
   }
   // ------------------------------------------------------------------------
   void Server::receive_msg()
   {
      socket.async_receive_from(
          buffer(tmpMsgIn.body, rft::PACKET_SIZE), remote_endpoint,
          boost::bind(&Server::handle_receive, this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Server::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Server] Received message";
         PLOG_INFO << "\"" + std::string(tmpMsgIn.body) + "\"";

         std::string msg = std::string("Echoing: ") + tmpMsgIn.body;
         send_msg_to_client(msg, remote_endpoint);
      } else {
         PLOG_WARNING << "[Server] Error on Receive: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Server::send_msg_to_client(std::string msg, ip::udp::endpoint client)
   {
      // change remote_endpoint to parameter client
      socket.async_send_to(buffer(msg, msg.size()), client,
                           boost::bind(&Server::handle_send, this, msg,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));

      receive_msg();
   }
   // ------------------------------------------------------------------------
   void Server::handle_send(const std::string& msg, const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Server] Send message";
         PLOG_INFO << "\"" + msg + "\"";
      } else {
         PLOG_WARNING << "[Server] Error on Send: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
