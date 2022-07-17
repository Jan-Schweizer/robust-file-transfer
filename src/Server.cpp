// ------------------------------------------------------------------------
#include "Server.hpp"
#include "Bitfield.hpp"
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
      io_context.stop();
      if (thread_context.joinable()) thread_context.join();
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
            handle_transmission_request(msg);
            break;
         case RETRANSMISSION_REQUEST:
            handle_retransmission_request(msg);
            break;
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
      Window window(maxWindowSize);
      // The first window id is 0. Therefore, the initial id is set to 0 - 1
      window.id = 0 - 1;

      connections.insert({connectionId, Connection{msg.header.remote, std::move(file), fileSize, maxWindowSize, std::move(window)}});

      // Build Initial Response Packet with file metadata
      tmpMsgOut.header.type = ServerMsgType::SERVER_INITIAL_RESPONSE;
      tmpMsgOut.header.size = 0;
      tmpMsgOut.header.remote = socket.local_endpoint();

      tmpMsgOut << ServerMsgType::SERVER_INITIAL_RESPONSE;
      tmpMsgOut << connectionId;
      tmpMsgOut << fileSize;
      tmpMsgOut << sha256;
      tmpMsgOut << filename;

      send_msg_to_client(tmpMsgOut, msg.header.remote);
   }
   // ------------------------------------------------------------------------
   void Server::handle_transmission_request(Message<ClientMsgType>& msg)
   {
      ConnectionID connectionId;
      uint8_t windowId;
      uint32_t chunkIdx;

      msg >> chunkIdx;
      msg >> windowId;
      msg >> connectionId;

      auto search = connections.find(connectionId);
      if (search == connections.end()) {
         // TODO: resume connection (remember to advance file idx pointer) & logging
         return;
      }
      auto& conn = search->second;

      if (windowId != static_cast<uint8_t>(conn.window.id + 1)) {
         // TODO: handle duplicate & logging
         return;
      }

      // Connection Migration: Every time a request for a connection is received, update the endpoint information for that connection
      conn.client = msg.header.remote;
      conn.window.id = windowId;
      conn.chunksWritten = chunkIdx;

      PLOG_INFO << "[Server] Transmission Request for connection ID " << connectionId << " at chunk index " << conn.chunksWritten;

      tmpMsgOut.header.type = ServerMsgType::PAYLOAD;
      tmpMsgOut.header.remote = socket.local_endpoint();

      for (uint16_t i = 0; i < conn.window.currentSize; ++i) {
         // read chunk from file
         unsigned char buffer[CHUNK_SIZE];
         conn.file.read(reinterpret_cast<char*>(buffer), CHUNK_SIZE);
         const size_t numBytesRead = conn.file.gcount();
         std::vector<unsigned char> chunk(std::begin(buffer), std::begin(buffer) + numBytesRead);

         // Read the last chunk of the file
         if (numBytesRead < CHUNK_SIZE) {
            conn.window.currentSize = i + 1;
         }

         tmpMsgOut.header.size = 0;

         tmpMsgOut << ServerMsgType::PAYLOAD;
         tmpMsgOut << connectionId;
         tmpMsgOut << conn.window.id;
         tmpMsgOut << conn.window.currentSize;
         tmpMsgOut << i;
         tmpMsgOut << chunk;

         conn.window.store_chunk(chunk, i);

         send_msg_to_client(tmpMsgOut, msg.header.remote);
      }

      // TODO: update congestion control information
   }
   // ------------------------------------------------------------------------
   void Server::handle_retransmission_request(Message<ClientMsgType>& msg)
   {
      uint16_t payloadSize = msg.header.size - (sizeof(ClientMsgType) + sizeof(ConnectionID) + sizeof(uint8_t));

      ConnectionID connectionId;
      uint8_t windowId;
      std::vector<unsigned char> payload(payloadSize);

      msg >> payload;
      msg >> windowId;
      msg >> connectionId;

      PLOG_INFO << "[Server] Received Retransmission Request for connection ID " << connectionId;

      auto& conn = connections.at(connectionId);

      // Connection Migration: Every time a request for a connection is received, update the endpoint information for that connection
      conn.client = msg.header.remote;

      Bitfield bitfield(conn.window.currentSize);
      bitfield.from(payload.data());

      tmpMsgOut.header.type = ServerMsgType::PAYLOAD;
      tmpMsgOut.header.remote = socket.local_endpoint();

      for (uint16_t i = 0; i < conn.window.currentSize; ++i) {
         if (!bitfield[i]) {
            tmpMsgOut.header.size = 0;

            tmpMsgOut << ServerMsgType::PAYLOAD;
            tmpMsgOut << connectionId;
            tmpMsgOut << conn.window.id;
            tmpMsgOut << conn.window.currentSize;
            tmpMsgOut << i;
            tmpMsgOut << conn.window.chunks[i];

            send_msg_to_client(tmpMsgOut, msg.header.remote);
         }
      }

      // TODO: update congestion control information
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
