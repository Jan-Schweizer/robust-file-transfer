#ifndef ROBUST_FILE_TRANSFER_CLIENT_HPP
#define ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
#include "MessageQueue.hpp"
#include "Window.hpp"
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
      class FileRequest
      {
         friend class Client;

         FileRequest(std::string& filename, boost::asio::io_context& io_context)
             : filename(filename), t(io_context) {}

         std::string& filename;
         boost::asio::steady_timer t;
         timepoint tp;
         uint8_t retryCounter = 1;
         const uint8_t maxRetries = 10;
      };
      // ------------------------------------------------------------------------
      class Connection
      {
         friend class Client;
         Connection(std::string& fileName, uint64_t fileSize, unsigned char sha256[SHA256_SIZE], Window window, boost::asio::io_context& io_context)
             : filename(std::move(fileName)), fileSize(fileSize), window(std::move(window)), t(io_context)
         {
            std::memcpy(this->sha256, sha256, SHA256_SIZE);
            file.open(this->filename, std::ios::binary | std::ios::trunc);
            if (!file) {
               PLOG_ERROR << "[Client] Could not open file for writing";
               // TODO: find a way to communicate this error and terminate file transfer
            }
         }

         std::string filename;
         std::ofstream file;
         uint64_t fileSize = 0;
         uint32_t bytesWritten = 0;
         unsigned char sha256[SHA256_SIZE]{'\0'};
         Window window;
         uint32_t chunksWritten = 0;
         uint16_t chunksReceivedInWindow = 0;

         boost::asio::steady_timer t;
         timepoint tp;
         bool shouldMeasureTime = true;
         uint8_t retryCounter = 1;
         const uint8_t maxRetries = 10;
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

      void set_timeout_for_file_request(std::string& filename);
      void set_timeout_for_transmission(ConnectionID connectionId);
      void set_timeout_for_retransmission(ConnectionID connectionId);

      void handle_validation_request(Message<ServerMsgType>& msg);
      void handle_initial_response(Message<ServerMsgType>& msg);
      void handle_payload_packet(Message<ServerMsgType>& msg);
      void handle_file_request_timeout(std::string& filename);
      void handle_transmission_timeout(ConnectionID connectionId);
      void handle_retransmission_timeout(ConnectionID connectionId);

      void request_transmission(ConnectionID connectionId);
      void request_retransmission(ConnectionID connectionId);
      void send_finish_msg(ConnectionID connectionId);

      boost::asio::io_context io_context;
      boost::asio::ip::udp::socket socket;
      std::thread thread_context;
      std::string host;
      size_t port;
      boost::asio::ip::udp::endpoint server_endpoint;
      boost::asio::ip::udp::endpoint remote_endpoint;

      std::string fileDest;
      std::unordered_map<ConnectionID, Connection> connections;
      std::unordered_map<std::string, FileRequest> fileRequests;

      /// A constant that is multiplied to the average rtt
      /// E.g. setting the timeout to be 10 times longer than the average rtt
      const size_t TIMEOUT = 10;
      uint32_t rttTotal = 0;
      size_t rttCount = 1;
      uint32_t rttCurrent = 0;

      Message<ServerMsgType> tmpMsgIn{};
      Message<ClientMsgType> tmpMsgOut{};
      MessageQueue<Message<ServerMsgType>> msgQueue;

      bool done = false;
   };
}
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
