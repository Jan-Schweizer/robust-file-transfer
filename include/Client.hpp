#ifndef ROBUST_FILE_TRANSFER_CLIENT_HPP
#define ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
#include "MessageQueue.hpp"
#include "Timer.hpp"
#include "Window.hpp"
#include "common.hpp"
#include "util.hpp"
#include <filesystem>
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

         explicit FileRequest(boost::asio::io_context& io_context) : timer(io_context) {}

         uint32_t nonce = 0;
         unsigned char hash1Solution[SHA256_SIZE]{'\0'};
         Timer timer;
         timepoint tp;
         uint8_t retryCounter = 1;
         const uint8_t maxRetries = 10;
      };
      // ------------------------------------------------------------------------
      class Connection
      {
         friend class Client;
         Connection(std::string& filename, uint64_t fileSize, unsigned char sha256[SHA256_SIZE], Window window, boost::asio::io_context& io_context)
             : filename(std::move(filename)), fileSize(fileSize), window(std::move(window)), timer(io_context)
         {
            std::memcpy(this->sha256, sha256, SHA256_SIZE);
            file.open(this->filename, std::ios::binary | std::ios::trunc);
            if (!file) {
               PLOG_ERROR << "[Client] Could not open file for writing.";
               throw std::system_error();
            }
         }

         std::string filename;
         std::ofstream file;
         uint64_t fileSize = 0;
         uint32_t bytesWritten = 0;
         uint32_t chunksWritten = 0;
         unsigned char sha256[SHA256_SIZE]{'\0'};
         Window window;

         Timer timer;
         timepoint tp;
         bool shouldMeasureTime = true;
         uint8_t retryCounter = 1;
         const uint8_t maxRetries = 10;

         bool isFileTransferComplete() const
         {
            return bytesWritten == fileSize;
         }
      };
      // ------------------------------------------------------------------------

    public:
      Client(std::string host, size_t port, std::string& fileDest, double p, double q);
      Client(const Client& other) = delete;
      Client(const Client&& other) = delete;
      ~Client();

      void request_files(std::vector<std::string>& files);

    private:
      void resolve_server();
      void start();
      void stop();

      void request_file(std::string filename);

      static void handle_user_termination();
      void delete_incomplete_files();

      void process_msgs();

      void dispatch_msg(Message<ServerMsgType>& msg);

      void send_msg(Message<ClientMsgType> msg);
      void receive_msg();

      void handle_receive(const boost::system::error_code& error, size_t bytes_transferred);
      void handle_send(const boost::system::error_code& error, size_t bytes_transferred);

      void enqueue_msg(size_t bytes_transferred);
      void decode_msg(size_t bytes_transferred);

      void handle_validation_request(Message<ServerMsgType>& msg);
      void handle_initial_response(Message<ServerMsgType>& msg);
      void handle_payload_packet(Message<ServerMsgType>& msg);
      void handle_validation_failed(Message<ServerMsgType>& msg);
      void handle_file_not_found(Message<ServerMsgType>& msg);
      void handle_connection_not_found(Message<ServerMsgType>& msg);

      void handle_file_request_timeout(std::string& filename);
      void handle_validation_response_timeout(std::string& filename);
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

      Message<ServerMsgType> msgIn{};
      MessageQueue<Message<ServerMsgType>> msgQueue;

      bool done = false;

      static std::sig_atomic_t abort;

      PacketLossState packetLossState = PacketLossState::NOT_LOST;
      double p;
      double q;
   };
}
// ------------------------------------------------------------------------
#endif//ROBUST_FILE_TRANSFER_CLIENT_HPP
// ------------------------------------------------------------------------
