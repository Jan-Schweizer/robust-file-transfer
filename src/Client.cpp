// ------------------------------------------------------------------------
#include "Client.hpp"
#include "Bitfield.hpp"
#include "util.hpp"
#include <boost/bind/bind.hpp>
// ------------------------------------------------------------------------
namespace rft
{
   // ------------------------------------------------------------------------
   Client::Client(std::string host, const size_t port, std::string& fileDest)
       : socket(io_context, ip::udp::endpoint(ip::udp::v4(), port + 1)), host(std::move(host)), port(port), fileDest(std::move(fileDest))
   {
      resolve_server();
   }
   // ------------------------------------------------------------------------
   Client::~Client() { stop(); }
   // ------------------------------------------------------------------------
   void Client::resolve_server()
   {
      try {
         ip::udp::resolver resolver(io_context);
         server_endpoint = *resolver.resolve(ip::udp::v4(), host, std::to_string(port)).begin();

         PLOG_INFO << "[Client] Resolved server at " + server_endpoint.address().to_string() + ":" + std::to_string(server_endpoint.port());
      } catch (std::exception& e) {
         PLOG_ERROR << "[Client] Error while trying to resolve_server to server";
         throw e;
      }
   }
   // ------------------------------------------------------------------------
   void Client::start()
   {
      receive_msg();
      thread_context = std::thread([this]() { io_context.run(); });

      process_msgs();
   }
   // ------------------------------------------------------------------------
   void Client::stop()
   {
      io_context.stop();
      if (thread_context.joinable()) thread_context.join();
      PLOG_INFO << "[Client] Disconnected!";
   }
   // ------------------------------------------------------------------------
   void Client::request_files(std::vector<std::string>& files)
   {
      for (auto& file: files) {
         request_file(file);
      }

      start();
   }
   // ------------------------------------------------------------------------
   void Client::request_file(std::string& filename)
   {
      tmpMsgOut.header.type = ClientMsgType::FILE_REQUEST;
      tmpMsgOut.header.size = 0;
      tmpMsgOut.header.remote = socket.local_endpoint();

      // Initial ConnectionId for a file request is set to 0
      ConnectionID connectionId = 0;

      tmpMsgOut << ClientMsgType::FILE_REQUEST;
      tmpMsgOut << connectionId;
      tmpMsgOut << MAX_WINDOW_SIZE;
      tmpMsgOut << filename;

      PLOG_INFO << "[Client] Requesting file: " << filename;

      fileRequests.insert({filename, FileRequest{filename, io_context}});

      set_timeout_for_file_request(filename);
      fileRequests.at(filename).tp = NOW;

      send_msg(tmpMsgOut);
   }
   // ------------------------------------------------------------------------
   void Client::send_msg(Message<ClientMsgType> msg)
   {
      socket.async_send_to(buffer(msg.packet, msg.header.size), server_endpoint,
                           boost::bind(&Client::handle_send, this,
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Client::handle_send(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Client] Send message";
      } else {
         PLOG_WARNING << "[Client] Error on Send: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Client::receive_msg()
   {
      socket.async_receive_from(buffer(tmpMsgIn.packet, MAX_PACKET_SIZE), remote_endpoint,
                                boost::bind(&Client::handle_receive, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Client::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
   {
      if (!error) {
         PLOG_INFO << "[Client] Received message";
         enqueue_msg(bytes_transferred);
      } else {
         PLOG_WARNING << "[Client] Error on Receive: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Client::enqueue_msg(size_t bytes_transferred)
   {
      decode_msg(bytes_transferred);
      msgQueue.push_back(tmpMsgIn);

      receive_msg();
   }
   // ------------------------------------------------------------------------
   void Client::decode_msg(size_t bytes_transferred)
   {
      auto& msg = tmpMsgIn.packet;

      auto msgType = static_cast<ServerMsgType>(msg[0]);

      tmpMsgIn.header.type = msgType;
      tmpMsgIn.header.size = bytes_transferred;
      tmpMsgIn.header.remote = remote_endpoint;
   }
   // ------------------------------------------------------------------------
   void Client::process_msgs()
   {
      while (!done) {
         msgQueue.wait();

         while (!msgQueue.empty()) {
            auto msg = msgQueue.pop_front();
            dispatch_msg(msg);
         }
      }
   }
   // ------------------------------------------------------------------------
   void Client::dispatch_msg(Message<ServerMsgType>& msg)
   {
      switch (msg.header.type) {
         case SERVER_INITIAL_RESPONSE:
            handle_initial_response(msg);
            break;
         case PAYLOAD:
            handle_payload_packet(msg);
            break;
         case SERVER_ERROR:
            break;
         // Ignore unknown packets
         default:
            return;
      }
   }
   // ------------------------------------------------------------------------
   void Client::handle_initial_response(Message<ServerMsgType>& msg)
   {
      auto end = NOW;

      size_t filenameSize = msg.header.size - (sizeof(ServerMsgType) + sizeof(ConnectionID) + sizeof(uint32_t) + SHA256_SIZE + 1);

      ConnectionID connectionId;
      uint32_t fileSize;
      unsigned char sha256[SHA256_SIZE];
      std::string filename(filenameSize, '\0');

      msg >> filename;
      msg >> sha256;
      msg >> fileSize;
      msg >> connectionId;

      auto duration = boost::asio::chrono::duration_cast<TIME_UNIT>(end - fileRequests.at(filename).tp);
      ++rttCount;
      rttCurrent = duration.count();
      rttTotal += rttCurrent;

      fileRequests.erase(filename);

      PLOG_INFO << "[Client] Got Initial response for file: " << filename;

      std::string dest = fileDest + "/" + filename;
      Window window{MAX_WINDOW_SIZE};
      connections.insert({connectionId, Connection{dest, fileSize, sha256, std::move(window), io_context}});

      request_transmission(connectionId);
   }
   // ------------------------------------------------------------------------
   void Client::handle_payload_packet(Message<ServerMsgType>& msg)
   {
      auto end = NOW;

      uint16_t chunkSize = msg.header.size - (PAYLOAD_META_DATA_SIZE);

      ConnectionID connectionId;
      uint8_t windowId;
      uint16_t currentWindowSize;
      uint16_t sequenceNumber;
      std::vector<unsigned char> chunk(chunkSize);

      msg >> chunk;
      msg >> sequenceNumber;
      msg >> currentWindowSize;
      msg >> windowId;
      msg >> connectionId;

      auto search = connections.find(connectionId);
      if (search == connections.end()) {
         // Ignore unknown connection id
         return;
      }
      auto& conn = search->second;

      if (conn.shouldMeasureTime) {
         auto duration = boost::asio::chrono::duration_cast<TIME_UNIT>(end - conn.tp);
         ++rttCount;
         rttCurrent = duration.count();
         rttTotal += rttCurrent;
         conn.shouldMeasureTime = false;
      }

      if (windowId != conn.window.id) {
         // Ignore delayed packets
         return;
      }

      set_timeout_for_retransmission(connectionId);
      // Server did respond -> reset retry counter
      conn.retryCounter = 1;

      conn.window.store_chunk(chunk, sequenceNumber);
      conn.window.currentSize = currentWindowSize;
      ++conn.chunksReceivedInWindow;

      if (conn.chunksReceivedInWindow == conn.window.currentSize) {
         // TODO: maybe extract this to separate function
         conn.t.cancel();

         uint32_t bytesWritten = 0;
         for (size_t i = 0; i < currentWindowSize; ++i) {
            uint32_t bytes = conn.window.chunks[i].size();
            conn.file.write(reinterpret_cast<char*>(conn.window.chunks[i].data()), bytes);
            // TODO: check for badbit and send no space left error message
            bytesWritten += bytes;
         }
         conn.bytesWritten += bytesWritten;
         conn.chunksWritten += currentWindowSize;
         conn.file.flush();

         PLOG_INFO << "[Client] Written " << currentWindowSize << " chunk" << ((currentWindowSize > 1) ? "s" : "")
                   << "(" << bytesWritten << "B)"
                   << " to disk";

         if (conn.bytesWritten == conn.fileSize) {
            unsigned char sha256[SHA256_SIZE];
            compute_SHA256(conn.filename, sha256);
            if (std::strncmp(reinterpret_cast<char*>(conn.sha256), reinterpret_cast<char*>(sha256), SHA256_SIZE) != 0) {
               PLOG_ERROR << "[Client] File " << conn.filename << " was not transferred successfully (wrong SHA256 checksum)\nPlease request file again!";
               connections.erase(connectionId);
               done = connections.empty() && fileRequests.empty();
               return;
            }

            PLOG_INFO << "[Client] Transferred file " << conn.filename << " successfully";
            connections.erase(connectionId);
            done = connections.empty() && fileRequests.empty();
            send_finish_msg(connectionId);
            return;
         }

         ++conn.window.id;
         request_transmission(connectionId);
      }
   }
   // ------------------------------------------------------------------------
   void Client::request_transmission(ConnectionID connectionId)
   {
      set_timeout_for_transmission(connectionId);

      tmpMsgOut.header.type = ClientMsgType::TRANSMISSION_REQUEST;
      tmpMsgOut.header.size = 0;
      tmpMsgOut.header.remote = socket.local_endpoint();

      auto& conn = connections.at(connectionId);
      tmpMsgOut << ClientMsgType::TRANSMISSION_REQUEST;
      tmpMsgOut << connectionId;
      tmpMsgOut << conn.window.id;
      tmpMsgOut << rttCurrent;
      tmpMsgOut << conn.chunksWritten;

      conn.chunksReceivedInWindow = 0;

      PLOG_INFO << "[Client] Requesting chunks at index: " << conn.chunksWritten << " for file " << conn.filename;

      conn.shouldMeasureTime = true;
      conn.tp = NOW;

      send_msg(tmpMsgOut);
   }
   // ------------------------------------------------------------------------
   void Client::request_retransmission(ConnectionID connectionId)
   {
      set_timeout_for_retransmission(connectionId);

      tmpMsgOut.header.type = ClientMsgType::RETRANSMISSION_REQUEST;
      tmpMsgOut.header.size = 0;
      tmpMsgOut.header.remote = socket.local_endpoint();

      auto& conn = connections.at(connectionId);

      Bitfield bitfield(conn.window.currentSize);
      bitfield.from(conn.window.sequenceNumbers);

      tmpMsgOut << ClientMsgType::RETRANSMISSION_REQUEST;
      tmpMsgOut << connectionId;
      tmpMsgOut << conn.window.id;
      tmpMsgOut << bitfield.bitfield;

      PLOG_INFO << "[Client] Requesting retransmission for connection ID " << connectionId;

      conn.shouldMeasureTime = true;
      conn.tp = NOW;

      send_msg(tmpMsgOut);
   }
   // ------------------------------------------------------------------------
   void Client::send_finish_msg(ConnectionID connectionId)
   {
      tmpMsgOut.header.type = ClientMsgType::FINISH;
      tmpMsgOut.header.size = 0;
      tmpMsgOut.header.remote = socket.local_endpoint();

      tmpMsgOut << ClientMsgType::FINISH;
      tmpMsgOut << connectionId;

      send_msg(tmpMsgOut);
   }
   // ------------------------------------------------------------------------
   void Client::set_timeout_for_file_request(std::string& filename)
   {
      auto& fr = fileRequests.at(filename);
      // Set a long timeout for a file request (calculation of SHA256 can take a while)
      fr.t.expires_after(boost::asio::chrono::minutes(10));
      fr.t.async_wait(boost::bind(&Client::handle_file_request_timeout, this, filename));
   }
   // ------------------------------------------------------------------------
   void Client::handle_file_request_timeout(std::string& filename)
   {
      auto search = fileRequests.find(filename);
      if (search != fileRequests.end()) {
         auto& fr = search->second;

         if (fr.retryCounter >= fr.maxRetries) {
            PLOG_ERROR << "[Client] Requested file " << filename << " multiple times without success.";
            fileRequests.erase(filename);
            done = fileRequests.empty() && connections.empty();
            // wake the main thread that is waiting on the msqQueue condition variable
            msgQueue.wake();
            return;
         }

         if (fr.t.expiry() <= steady_timer::clock_type::now()) {
            PLOG_INFO << "[Client] Repeating request for file: " << filename;
            ++fr.retryCounter;
            request_file(filename);
         }
      }
   }
   // ------------------------------------------------------------------------
   void Client::set_timeout_for_transmission(ConnectionID connectionId)
   {
      auto& conn = connections.at(connectionId);
      conn.t.expires_after(TIME_UNIT(rttTotal/ rttCount * TIMEOUT));
      conn.t.async_wait(boost::bind(&Client::handle_transmission_timeout, this, connectionId));
   }
   // ------------------------------------------------------------------------
   void Client::handle_transmission_timeout(ConnectionID connectionId)
   {
      auto search = connections.find(connectionId);
      if (search != connections.end()) {
         auto& conn = search->second;

         if (conn.retryCounter > conn.maxRetries) {
            PLOG_ERROR << "[Client] Sent multiple Transmission Requests. Server may have disconnected.";
            connections.erase(connectionId);
            done = fileRequests.empty() && connections.empty();
            // wake the main thread that is waiting on the msqQueue condition variable
            msgQueue.wake();
            return;
         }

         if (conn.t.expiry() <= steady_timer::clock_type::now()) {
            PLOG_INFO << "[Client] Repeating Transmission Request for " << connectionId;
            ++conn.retryCounter;
            request_transmission(connectionId);
         }
      }
   }
   // ------------------------------------------------------------------------
   void Client::set_timeout_for_retransmission(ConnectionID connectionId)
   {
      auto& conn = connections.at(connectionId);
      conn.t.expires_after(TIME_UNIT(rttTotal/ rttCount * TIMEOUT));
      conn.t.async_wait(boost::bind(&Client::handle_retransmission_timeout, this, connectionId));
   }
   // ------------------------------------------------------------------------
   void Client::handle_retransmission_timeout(ConnectionID connectionId)
   {
      auto search = connections.find(connectionId);
      if (search != connections.end()) {
         auto& conn = search->second;

         if (conn.retryCounter > conn.maxRetries) {
            PLOG_ERROR << "[Client] Sent multiple Retransmission Requests. Server may have disconnected.";
            connections.erase(connectionId);
            done = fileRequests.empty() && connections.empty();
            // wake the main thread that is waiting on the msqQueue condition variable
            msgQueue.wake();
            return;
         }

         if (conn.t.expiry() <= steady_timer::clock_type::now()) {
            PLOG_INFO << "[Client] Retransmission Request for " << connectionId;
            ++conn.retryCounter;
            request_retransmission(connectionId);
         }
      }
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
