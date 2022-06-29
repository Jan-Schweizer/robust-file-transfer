#ifndef ROBUST_FILE_TRANSFER_SERVER_HPP
#define ROBUST_FILE_TRANSFER_SERVER_HPP
// ------------------------------------------------------------------------
#include "MessageQueue.hpp"
#include "common.hpp"
#include <fstream>
#include <unordered_map>
#include <utility>
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

         FileTransfer(boost::asio::ip::udp::endpoint client, std::ifstream file, uint32_t fileSize, unsigned char sha256[SHA256_SIZE], uint8_t maxWindowSize)
             : client(std::move(client)), file(std::move(file)), fileSize(fileSize), maxWindowSize(maxWindowSize)
         {
            std::memcpy(this->sha256, sha256, SHA256_SIZE);
         }

         boost::asio::ip::udp::endpoint client;
         std::ifstream file;
         uint32_t fileSize;
         unsigned char sha256[SHA256_SIZE]{'\0'};
         uint8_t maxWindowSize;
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
      [[noreturn]] void process_msgs();

      void dispatch_msg(Message<ClientMsgType>& msg);

      void receive_msg();
      void send_msg_to_client(Message<ServerMsgType> msg, const boost::asio::ip::udp::endpoint& client);

      void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
      void handle_send(const boost::system::error_code& error, size_t bytes_transferred);

      void enqueue_msg(size_t bytes_transferred);
      void decode_msg(size_t bytes_transferred);

      void handle_file_request(Message<ClientMsgType>& msg);

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
