#ifndef ROBUST_FILE_TRANSFER_CLIENT_HPP
#define ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
#include <boost/asio.hpp>
// ------------------------------------------------------------------------
namespace rft
// ------------------------------------------------------------------------
{
   class Client
   {
    public:
      Client(boost::asio::io_context& io_context, std::string host, size_t port);
      Client(const Client& other) = delete;
      Client(const Client&& other) = delete;
      ~Client();

      void send_msg(char msg[512]);
      void recv_msg();

    private:
      bool resolve_server();
      void stop();

      void handle_receive(const boost::system::error_code& error, boost::asio::ip::udp::endpoint& client, size_t bytes_transferred);
      void handle_send(const std::string& msg, const boost::system::error_code& error, size_t bytes_transferred);

      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket socket;
      std::string host;
      size_t port;
      boost::asio::ip::udp::endpoint server_endpoint;
      boost::asio::ip::udp::endpoint remote_endpoint;
      char recv_buffer[512]{};
   };
}
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
