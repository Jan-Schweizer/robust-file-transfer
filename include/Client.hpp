#ifndef ROBUST_FILE_TRANSFER_CLIENT_HPP
#define ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
#include "MessageQueue.hpp"
#include "common.hpp"
#include <fstream>
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

         FileTransfer(std::string& fileName, uint32_t fileSize, unsigned char sha256[SHA256_SIZE])
             : fileName(std::move(fileName)), fileSize(fileSize)
         {
            std::memcpy(this->sha256, sha256, SHA256_SIZE);
            file.open(this->fileName, std::ios::binary | std::ios::out | std::ios::app | std::ios::trunc);
         }

         std::string fileName;
         std::ofstream file;
         uint32_t fileSize;
         unsigned char sha256[SHA256_SIZE]{'\0'};
         bool done = false;
      };
      // ------------------------------------------------------------------------

    public:
      Client(std::string host, size_t port, std::string& fileDest);
      Client(const Client& other) = delete;
      Client(const Client&& other) = delete;
      ~Client();

      void request_files(std::vector<std::string>& files);

    private:
      void resolve_server();
      void start();
      void stop();

      void request_file(std::string& filename);

      void process_msgs();

      void dispatch_msg(Message<ServerMsgType>& msg);

      void send_msg(Message<ClientMsgType> msg);
      void receive_msg();

      void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
      void handle_send(const boost::system::error_code& error, size_t bytes_transferred);

      void enqueue_msg(size_t bytes_transferred);
      void decode_msg(size_t bytes_transferred);

      void handle_initial_response(Message<ServerMsgType>& msg);

      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket socket;
      std::thread thread_context;
      std::string host;
      size_t port;
      boost::asio::ip::udp::endpoint server_endpoint;
      boost::asio::ip::udp::endpoint remote_endpoint;

      std::string fileDest;
      std::unordered_map<ConnectionID, FileTransfer> fileTransfers;

      Message<ServerMsgType> tmpMsgIn{};
      Message<ClientMsgType> tmpMsgOut{};
      MessageQueue<Message<ServerMsgType>> msgQueue;
      bool done = false;
   };
}
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
