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
    public:
      Server(boost::asio::io_context& io_context, size_t port);
      Server(const Server& other) = delete;
      Server(const Server&& other) = delete;
      ~Server();

      void start();
      void stop();

    private:
      // ------------------------------------------------------------------------
      enum MsgType : uint8_t
      {
         // Standard Types
         PAYLOAD = 0b0000,

         // Error Types
         FILE_NOT_FOUND = 0b1000
      };
      // ------------------------------------------------------------------------
      class FileTransfer
      {
         friend class Server;

         boost::asio::ip::udp::endpoint client;
      };
      // ------------------------------------------------------------------------

      using connectionID = uint16_t;

      void receive_msg();
      void send_msg_to_client(std::string msg, boost::asio::ip::udp::endpoint client);

      void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
      void handle_send(const std::string& msg, const boost::system::error_code& error, size_t bytes_transferred);

      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket socket;
      size_t port;
      boost::asio::ip::udp::endpoint remote_endpoint;
      std::unordered_map<connectionID, FileTransfer> fileTransfers;

      Message<MsgType> tmpMsgIn;
      Message<MsgType> tmpMsgOut;
      MessageQueue<Message<MsgType>> messageQueue;
   };
}// namespace rft
// ------------------------------------------------------------------------

#endif//ROBUST_FILE_TRANSFER_SERVER_HPP
