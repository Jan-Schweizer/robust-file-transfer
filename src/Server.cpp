// ------------------------------------------------------------------------
#include "Server.hpp"
#include "Bitfield.hpp"
#include "CongestionControl.hpp"
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
          buffer(msgIn.packet, MAX_PACKET_SIZE), remote_endpoint,
          boost::bind(&Server::handle_receive, this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Server::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         // PLOG_VERBOSE << "[Server] Received message";
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
         // PLOG_VERBOSE << "[Server] Send message";
      } else {
         PLOG_WARNING << "[Server] Error on Send: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Server::enqueue_msg(size_t bytes_transferred)
   {
      decode_msg(bytes_transferred);
      msgQueue.push_back(msgIn);

      receive_msg();
   }
   // ------------------------------------------------------------------------
   void Server::decode_msg(size_t bytes_transferred)
   {
      auto& msg = msgIn.packet;

      auto msgType = static_cast<ClientMsgType>(msg[0]);

      msgIn.header.type = msgType;
      msgIn.header.size = bytes_transferred;
      msgIn.header.remote = remote_endpoint;
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
         // TODO: Think about which operation should be done on the main thread and which on a separate thread
         case FILE_REQUEST:
            handle_file_request(msg);
            break;
         case CLIENT_VALIDATION_RESPONSE:
            // validating the response is a time-consuming operation, do not block the main thread for this (otherwise timeouts for file transfers that are already in progress will fire)
            post(boost::bind(&Server::handle_validation_response, this, msg));
            break;
         case TRANSMISSION_REQUEST:
            handle_transmission_request(msg);
            break;
         case RETRANSMISSION_REQUEST:
            handle_retransmission_request(msg);
            break;
         case CLIENT_FINISH_MESSAGE:
            handle_finish(msg);
            break;
         case ERROR_CONNECTION_TERMINATION:
            break;
         // Ignore unknown packets
         default:
            return;
      }
   }
   // ------------------------------------------------------------------------
   void Server::handle_file_request(Message<ClientMsgType>& msg)
   {
      uint32_t filenameSize = msg.header.size - (sizeof(ClientMsgType) + 1);

      unsigned char hash1[SHA256_SIZE];
      unsigned char hash2[SHA256_SIZE];
      uint32_t nonce = chrono::duration_cast<seconds>(NOW.time_since_epoch()).count();
      std::string filename(filenameSize, '\0');

      msg >> filename;

      std::string str(std::to_string(nonce) + filename + SERVER_SECRET);
      compute_SHA256(reinterpret_cast<unsigned char*>(str.data()), str.size(), hash1);
      compute_SHA256(hash1, SHA256_SIZE, hash2);

      // mask difficulty-number of bits from hash1
      uint8_t remaining = DIFFICULTY;
      uint8_t byte = SHA256_SIZE -1;
      while (remaining >= 8) {
         hash1[byte] = 0;
         --byte;
         remaining -= 8;
      }
      hash1[byte] &= 0b11111111 << remaining;

      Message<ServerMsgType> msgOut;
      msgOut.header.type = SERVER_VALIDATION_REQUEST;
      msgOut.header.size = 0;
      msgOut.header.remote = socket.local_endpoint();

      msgOut << SERVER_VALIDATION_REQUEST;
      msgOut << DIFFICULTY;
      msgOut << hash1;
      msgOut << hash2;
      msgOut << nonce;
      msgOut << filename;

      PLOG_INFO << "[Server] Client requesting file: " << filename;

      send_msg_to_client(msgOut, msg.header.remote);
   }
   // ------------------------------------------------------------------------
   void Server::handle_validation_response(Message<ClientMsgType>& msg)
   {
      uint32_t filenameSize = msg.header.size - CLIENT_VALIDATION_RESPONSE_META_DATA_SIZE;

      unsigned char hash1[SHA256_SIZE];
      unsigned char originalHash1[SHA256_SIZE];
      uint32_t nonce;
      uint16_t maxThroughput;
      std::string filename(filenameSize, '\0');

      msg >> filename;
      msg >> maxThroughput;
      msg >> nonce;
      msg >> hash1;

      // verify solution
      std::string str(std::to_string(nonce) + filename + SERVER_SECRET);
      compute_SHA256(reinterpret_cast<unsigned char*>(str.data()), str.size(), originalHash1);
      if (std::strncmp(reinterpret_cast<char*>(originalHash1), reinterpret_cast<char*>(hash1), SHA256_SIZE) != 0) {
         PLOG_WARNING << "[Server] Client did not pass validation for file: " << filename;

         Message<ServerMsgType> msgOut;
         msgOut.header.type = ERROR_CLIENT_VALIDATION_FAILED;
         msgOut.header.size = 0;
         msgOut.header.remote = socket.local_endpoint();

         msgOut << ERROR_CLIENT_VALIDATION_FAILED;
         msgOut << filename;

         send_msg_to_client(msgOut, msg.header.remote);
         return;
      }

      PLOG_INFO << "[Server] Client has passed validation for file: " << filename;

      std::ifstream file(filename, std::ios::in | std::ios::binary);
      if (!file) {
         PLOG_WARNING << "[Server] File: " << filename << " does not exist!";
         Message<ServerMsgType> msgOut;
         msgOut.header.type = ERROR_FILE_NOT_FOUND;
         msgOut.header.size = 0;
         msgOut.header.remote = socket.local_endpoint();

         msgOut << ERROR_FILE_NOT_FOUND;
         msgOut << filename;

         send_msg_to_client(msgOut, msg.header.remote);
         return;
      }

      ConnectionID connectionId = connectionIdPool++;
      uint64_t fileSize = std::filesystem::file_size(filename);
      unsigned char sha256[SHA256_SIZE];
      compute_file_SHA256(filename, sha256);
      Window window(maxThroughput * 1024 * 1024 / CHUNK_SIZE);

      connections.insert({connectionId, Connection{msg.header.remote, std::move(file), maxThroughput, std::move(window), io_context}});

      Message<ServerMsgType> msgOut;
      msgOut.header.type = SERVER_INITIAL_RESPONSE;
      msgOut.header.size = 0;
      msgOut.header.remote = socket.local_endpoint();

      msgOut << SERVER_INITIAL_RESPONSE;
      msgOut << connectionId;
      msgOut << fileSize;
      msgOut << sha256;
      msgOut << filename;

      set_timeout(connectionId);
      send_msg_to_client(msgOut, msg.header.remote);
   }
   // ------------------------------------------------------------------------
   void Server::handle_transmission_request(Message<ClientMsgType>& msg)
   {
      ConnectionID connectionId;
      uint8_t windowId;
      uint32_t rttCurrent;
      uint32_t chunkIdx;

      msg >> chunkIdx;
      msg >> rttCurrent;
      msg >> windowId;
      msg >> connectionId;

      auto search = connections.find(connectionId);
      if (search == connections.end()) {
         PLOG_WARNING << "No connection for: " << connectionId;

         Message<ServerMsgType> msgOut;
         msgOut.header.type = ERROR_CONNECTION_NOT_FOUND;
         msgOut.header.size = 0;
         msgOut.header.remote = socket.local_endpoint();

         msgOut << ERROR_CONNECTION_NOT_FOUND;
         msgOut << connectionId;

         send_msg_to_client(msgOut, msg.header.remote);
         return;
      }
      auto& conn = search->second;

      // Connection Migration: Every time a request for a connection is received, update the endpoint information for that connection
      conn.client = msg.header.remote;
      conn.window.id = windowId;

      PLOG_VERBOSE << "[Server] Transmission Request for connection ID " << connectionId << " at chunk index " << chunkIdx;

      Message<ServerMsgType> msgOut;
      msgOut.header.type = PAYLOAD;
      msgOut.header.remote = socket.local_endpoint();

      conn.window.currentSize = conn.cc.getNextWindowSize(rttCurrent);

      if (conn.cc.phase == CongestionControl::Phase::CC_AVOIDANCE) {
         conn.cc.phase = CongestionControl::Phase::CC_NORMAL;
      }

      // set offset into file
      conn.file.seekg(chunkIdx * CHUNK_SIZE);
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

         msgOut.header.size = 0;

         msgOut << PAYLOAD;
         msgOut << connectionId;
         msgOut << conn.window.id;
         msgOut << conn.window.currentSize;
         msgOut << i;
         msgOut << chunk;

         conn.window.store_chunk(chunk, i);

         set_timeout(connectionId);
         send_msg_to_client(msgOut, msg.header.remote);
      }
   }
   // ------------------------------------------------------------------------
   void Server::handle_finish(Message<ClientMsgType>& msg)
   {
      ConnectionID connectionId;

      msg >> connectionId;

      PLOG_INFO << "[Server] Received Finish message for connection ID " << connectionId;
      connections.erase(connectionId);
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

      auto search = connections.find(connectionId);
      if (search == connections.end()) {
         PLOG_WARNING << "No connection for: " << connectionId;

         Message<ServerMsgType> msgOut;
         msgOut.header.type = ERROR_CONNECTION_NOT_FOUND;
         msgOut.header.size = 0;
         msgOut.header.remote = socket.local_endpoint();

         msgOut << ERROR_CONNECTION_NOT_FOUND;
         msgOut << connectionId;

         send_msg_to_client(msgOut, msg.header.remote);
         return;
      }
      auto& conn = search->second;

      conn.cc.phase = CongestionControl::Phase::CC_AVOIDANCE;

      // Connection Migration: Every time a request for a connection is received, update the endpoint information for that connection
      conn.client = msg.header.remote;

      Bitfield bitfield(conn.window.currentSize);
      bitfield.from(payload.data());

      Message<ServerMsgType> msgOut;
      msgOut.header.type = PAYLOAD;
      msgOut.header.remote = socket.local_endpoint();

      for (uint16_t i = 0; i < conn.window.currentSize; ++i) {
         if (!bitfield[i]) {
            msgOut.header.size = 0;

            msgOut << PAYLOAD;
            msgOut << connectionId;
            msgOut << conn.window.id;
            msgOut << conn.window.currentSize;
            msgOut << i;
            msgOut << conn.window.chunks[i];

            set_timeout(connectionId);
            send_msg_to_client(msgOut, msg.header.remote);
         }
      }
   }
   // ------------------------------------------------------------------------
   void Server::set_timeout(ConnectionID connectionId)
   {
      auto& conn = connections.at(connectionId);

      conn.t.expires_after(minutes(TIMEOUT));
      conn.t.async_wait(boost::bind(&Server::handle_timeout, this, connectionId));
   }
   // ------------------------------------------------------------------------
   void Server::handle_timeout(ConnectionID connectionId)
   {
      auto search = connections.find(connectionId);
      if (search != connections.end()) {
         auto& conn = search->second;
         if (conn.t.expiry() <= steady_timer::clock_type::now()) {
            PLOG_INFO << "Timeout expired for: " << connectionId;
            connections.erase(connectionId);
         }
      }
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
