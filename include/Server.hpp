#ifndef ROBUST_FILE_TRANSFER_SERVER_HPP
#define ROBUST_FILE_TRANSFER_SERVER_HPP
// ------------------------------------------------------------------------
#include "MessageQueue.hpp"
#include <boost/asio.hpp>
#include <unordered_map>
// ------------------------------------------------------------------------
namespace rft
// ------------------------------------------------------------------------
{
   class Server
   {
      // ------------------------------------------------------------------------
      class FileTransfer
      {
         friend class Server;

         boost::asio::ip::udp::endpoint client;
      };
      // ------------------------------------------------------------------------

    public:
      explicit Server(size_t port);
      Server(const Server& other) = delete;
      Server(const Server&& other) = delete;
      ~Server();

      void start();
      void stop();

    private:
      void echo_msg(Message<ClientMsgType>& msg);

      void receive_msg();
      void send_msg_to_client(Message<ServerMsgType>& msg, const boost::asio::ip::udp::endpoint& client);

      void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
      void handle_send(const boost::system::error_code& error, size_t bytes_transferred);

      void decode_msg(size_t bytes_transferred);
      void enqueue_msg(size_t bytes_transferred);

      [[noreturn]] void process_msgs();

      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket socket;
      std::thread thread_context;
      size_t port;
      boost::asio::ip::udp::endpoint remote_endpoint;

      std::unordered_map<ConnectionID, FileTransfer> fileTransfers;
      ConnectionID connectionIdPool = 0;

      Message<ClientMsgType> tmpMsgIn{};
      Message<ServerMsgType> tmpMsgOut{};
      MessageQueue<Message<ClientMsgType>> msgQueue;
   };
}// namespace rft
// ------------------------------------------------------------------------

#endif//ROBUST_FILE_TRANSFER_SERVER_HPP
