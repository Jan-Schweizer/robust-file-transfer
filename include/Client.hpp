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
    public:
      Client(boost::asio::io_context& io_context, std::string host, size_t port);
      Client(const Client& other) = delete;
      Client(const Client&& other) = delete;
      ~Client();

      void send_msg(char msg[512]);
      void recv_msg();

    private:
      // ------------------------------------------------------------------------
      enum MsgType : uint8_t
      {
         // Standard Types
         FILE_REQUEST = 0b0000,

         // Error Types
         RETRANSMISSION_REQUEST = 0b1000
      };
      // ------------------------------------------------------------------------
      class FileTransfer
      {
         friend class Client;
         std::string file_name;
      };
      // ------------------------------------------------------------------------

      using connectionID = uint16_t;

      bool resolve_server();
      void stop();

      void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
      void handle_send(const std::string& msg, const boost::system::error_code& error, size_t bytes_transferred);

      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket socket;
      std::string host;
      size_t port;
      boost::asio::ip::udp::endpoint server_endpoint;
      boost::asio::ip::udp::endpoint remote_endpoint;
      std::unordered_map<connectionID, FileTransfer> fileTransfers;

      Message<MsgType> tmpMsgIn;
      Message<MsgType> tmpMsgOut;
      MessageQueue<Message<MsgType>> msgQueue;
   };
}
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
