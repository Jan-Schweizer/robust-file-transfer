// ------------------------------------------------------------------------
#include "Server.hpp"
#include "util.hpp"
#include <boost/bind/bind.hpp>
#include <filesystem>
#include <fstream>
// ------------------------------------------------------------------------
namespace rft
{
   // ------------------------------------------------------------------------
   Server::Server(const size_t port)
       : socket(io_context, ip::udp::endpoint(ip::udp::v4(), port)), port(port) {}
   // ------------------------------------------------------------------------
   Server::~Server() { stop(); }
   // ------------------------------------------------------------------------
   void Server::start()
   {
      receive_msg();
      thread_context = std::thread([this]() { io_context.run(); });
      PLOG_INFO << "[Server] Started on port " + std::to_string(port) + "!";

      process_msgs();
   }
   // ------------------------------------------------------------------------
   void Server::stop()
   {
      if (thread_context.joinable()) thread_context.join();
      io_context.stop();
      PLOG_INFO << "[Server] Stopped!";
   }
   // ------------------------------------------------------------------------
   void Server::receive_msg()
   {
      socket.async_receive_from(
          buffer(tmpMsgIn.packet, MAX_PACKET_SIZE), remote_endpoint,
          boost::bind(&Server::handle_receive, this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Server::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Server] Received message";
         enqueue_msg(bytes_transferred);
      } else {
         PLOG_WARNING << "[Server] Error on Receive: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Server::send_msg_to_client(Message<ServerMsgType> msg, const ip::udp::endpoint& client)
   {
      socket.async_send_to(buffer(msg.packet, msg.header.size), client,
                           boost::bind(&Server::handle_send, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Server::handle_send(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Server] Send message";
      } else {
         PLOG_WARNING << "[Server] Error on Send: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Server::enqueue_msg(size_t bytes_transferred)
   {
      decode_msg(bytes_transferred);
      msgQueue.push_back(tmpMsgIn);

      receive_msg();
   }
   // ------------------------------------------------------------------------
   void Server::decode_msg(size_t bytes_transferred)
   {
      auto& msg = tmpMsgIn.packet;

      auto msgType = static_cast<ClientMsgType>(msg[0]);

      tmpMsgIn.header.type = msgType;
      tmpMsgIn.header.size = bytes_transferred;
      tmpMsgIn.header.remote = remote_endpoint;
   }
   // ------------------------------------------------------------------------
   void Server::process_msgs()
   {
      while (true) {
         msgQueue.wait();

         while (!msgQueue.empty()) {
            auto msg = msgQueue.pop_front();
            dispatch_msg(msg);
         }
      }
   }
   // ------------------------------------------------------------------------
   void Server::dispatch_msg(Message<ClientMsgType>& msg)
   {
      switch (msg.header.type) {
         case FILE_REQUEST:
            handle_file_request(msg);
            break;
         case TRANSMISSION_REQUEST:
         case RETRANSMISSION_REQUEST:
         case CLIENT_ERROR:
            break;
         // Ignore unknown packets
         default:
            return;
      }
   }
   // ------------------------------------------------------------------------
   void Server::handle_file_request(Message<ClientMsgType>& msg)
   {
      uint16_t maxWindowSize;
      uint32_t filenameSize = msg.header.size - (sizeof(ClientMsgType) + sizeof(ConnectionID) + sizeof(MAX_WINDOW_SIZE) + 1);
      std::string filename(filenameSize, '\0');

      msg >> filename;
      msg >> maxWindowSize;

      PLOG_INFO << "[Server] Client requesting file: " << filename;

      std::ifstream file(filename, std::ios::in | std::ios::binary);
      if (!file) {
         PLOG_ERROR << "[Server] File: " << filename << " does not exist!";
         // TODO: send back error message
         return;
      }

      ConnectionID connectionId = connectionIdPool++;
      uint32_t fileSize = std::filesystem::file_size(filename);
      unsigned char sha256[SHA256_SIZE];
      compute_SHA256(filename, sha256);

      fileTransfers.insert({connectionId, FileTransfer{msg.header.remote, std::move(file), fileSize, sha256, maxWindowSize}});

      // Build Initial Response Packet with file metadata
      tmpMsgOut.header.type = ServerMsgType::SERVER_INITIAL_RESPONSE;
      tmpMsgOut.header.size = 0;
      tmpMsgOut.header.remote = socket.local_endpoint();

      tmpMsgOut << ServerMsgType::SERVER_INITIAL_RESPONSE;
      tmpMsgOut << connectionId;
      tmpMsgOut << fileSize;
      tmpMsgOut << sha256;
      tmpMsgOut << filename;

      hexdump(tmpMsgOut.packet, tmpMsgOut.header.size);

      send_msg_to_client(tmpMsgOut, msg.header.remote);
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
