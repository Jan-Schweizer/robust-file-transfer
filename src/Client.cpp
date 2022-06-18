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
      resolve_server();

      char msg[512] = "Hello from client";
      send_msg(msg);
   }
   // ------------------------------------------------------------------------
   Client::~Client() { stop(); }
   // ------------------------------------------------------------------------
   bool Client::resolve_server()
   {
      try {
         ip::udp::resolver resolver(io_context);
         server_endpoint = *resolver.resolve(ip::udp::v4(), host, std::to_string(port)).begin();

         PLOG_INFO << "[Client] Resolved server at  " + server_endpoint.address().to_string() + ":" + std::to_string(server_endpoint.port());
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
   void Client::send_msg(char msg[512])
   {
      socket.async_send_to(buffer(msg, 512), server_endpoint,
                           boost::bind(&Client::handle_send, this, std::string(msg),
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));

      recv_msg();
   }
   // ------------------------------------------------------------------------
   void Client::handle_send(const std::string& msg, const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Client] Send message";
         PLOG_INFO << "\"" + msg + "\"";
      } else {
         PLOG_WARNING << "[Client] Error on Send: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Client::recv_msg()
   {
      socket.async_receive_from(buffer(recv_buffer), remote_endpoint,
                                boost::bind(&Client::handle_receive, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Client::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Client] Received message";
         PLOG_INFO << "\"" + std::string(recv_buffer) + "\"";
      } else {
         PLOG_WARNING << "[Client] Error on Receive: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
