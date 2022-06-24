#ifndef ROBUST_FILE_TRANSFER_CLIENT_HPP
#define ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
#include "MessageQueue.hpp"
#include <boost/asio.hpp>
#include <unordered_map>
// ------------------------------------------------------------------------
namespace rft
// ------------------------------------------------------------------------
{
   class Client
   {
      // ------------------------------------------------------------------------
      class FileTransfer
      {
         friend class Client;
         std::string fileName;
         ConnectionID connectionId;
      };
      // ------------------------------------------------------------------------

    public:
      Client(std::string host, size_t port);
      Client(const Client& other) = delete;
      Client(const Client&& other) = delete;
      ~Client();

      void send_msg(Message<ClientMsgType>& msg);
      void recv_msg();

    private:
      void create_echo_msg();

      bool resolve_server();
      void start();
      void stop();

      void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
      void handle_send(const boost::system::error_code& error, size_t bytes_transferred);

      void decode_msg(size_t bytes_transferred);
      void enqueue_msg(size_t bytes_transferred);

      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket socket;
      std::thread thread_context;
      std::string host;
      size_t port;
      boost::asio::ip::udp::endpoint server_endpoint;
      boost::asio::ip::udp::endpoint remote_endpoint;

      std::unordered_map<ConnectionID, FileTransfer> fileTransfers;

      Message<ServerMsgType> tmpMsgIn{};
      Message<ClientMsgType> tmpMsgOut{};
      MessageQueue<Message<ServerMsgType>> msgQueue;
   };
}
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
