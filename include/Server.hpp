#ifndef ROBUST_FILE_TRANSFER_SERVER_HPP
#define ROBUST_FILE_TRANSFER_SERVER_HPP
// ------------------------------------------------------------------------
#include <boost/asio.hpp>
// ------------------------------------------------------------------------
namespace rft
// -----------------------------------------------------------------   // -------------------------------------------------------------------------------
{
   class Server
   {
    public:
      Server(boost::asio::io_context& io_context, size_t port);
      Server(const Server& other) = delete;
      Server(const Server&& other) = delete;
      ~Server();

      void start();
      void stop();

    private:

      void receive_msg();
      void send_msg_to_client(std::string msg, boost::asio::ip::udp::endpoint client);

      void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
      void handle_send(const std::string& msg, const boost::system::error_code& error, size_t bytes_transferred);

      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket socket;
      size_t port;
      boost::asio::ip::udp::endpoint remote_endpoint;
      char recv_buffer[512]{};
   };
}// namespace rft
// ------------------------------------------------------------------------

#endif//ROBUST_FILE_TRANSFER_SERVER_HPP
