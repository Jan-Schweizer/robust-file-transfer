// ------------------------------------------------------------------------
#include "Client.hpp"
#include "Bitfield.hpp"
#include "util.hpp"
#include <boost/bind/bind.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <csignal>
#include <filesystem>
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
   int Client::abort = 0;
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
      handle_user_termination();

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
   void Client::request_file(std::string filename)
   {
      Message<ClientMsgType> msgOut;
      msgOut.header.type = FILE_REQUEST;
      msgOut.header.size = 0;
      msgOut.header.remote = socket.local_endpoint();

      msgOut << FILE_REQUEST;
      msgOut << filename;

      PLOG_INFO << "[Client] Requesting file: " << filename;

      fileRequests.insert({filename, FileRequest{io_context}});

      set_timeout_for_file_request(filename);
      fileRequests.at(filename).tp = NOW;

      send_msg(msgOut);
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
         // PLOG_VERBOSE << "[Client] Send message";
      } else {
         PLOG_WARNING << "[Client] Error on Send: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Client::receive_msg()
   {
      socket.async_receive_from(buffer(msgIn.packet, MAX_PACKET_SIZE), remote_endpoint,
                                boost::bind(&Client::handle_receive, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
   }
   // ------------------------------------------------------------------------
   void Client::handle_receive(const boost::system::error_code& error, size_t bytes_transferred)
   {
      // don't accept more incoming packets if user aborted
      if (abort == 1) {
         for (auto& fr: fileRequests) {
            fr.second.t.cancel();
         }
         for (auto& conn: connections) {
            conn.second.t.cancel();
         }
         return;
      }

      if (!error) {
         // PLOG_VERBOSE << "[Client] Received message";
         enqueue_msg(bytes_transferred);
      } else {
         PLOG_WARNING << "[Client] Error on Receive: " + error.to_string();
      }
   }
   // ------------------------------------------------------------------------
   void Client::enqueue_msg(size_t bytes_transferred)
   {
      decode_msg(bytes_transferred);
      msgQueue.push_back(msgIn);

      receive_msg();
   }
   // ------------------------------------------------------------------------
   void Client::decode_msg(size_t bytes_transferred)
   {
      auto& msg = msgIn.packet;

      auto msgType = static_cast<ServerMsgType>(msg[0]);

      msgIn.header.type = msgType;
      msgIn.header.size = bytes_transferred;
      msgIn.header.remote = remote_endpoint;
   }
   // ------------------------------------------------------------------------
   void Client::process_msgs()
   {
      while (!done) {
         msgQueue.wait();

         if (abort == 1) {
            delete_incomplete_files();
            return;
         }

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
         // TODO: Think about which operation should be done on the main thread and which on a separate thread
         case SERVER_VALIDATION_REQUEST:
            // finding a solution is a time-consuming operation, do not block the main thread for this (otherwise timeouts for file transfers that are already in progress will fire)
            post(boost::bind(&Client::handle_validation_request, this, msg));
            break;
         case SERVER_INITIAL_RESPONSE:
            handle_initial_response(msg);
            break;
         case PAYLOAD:
            handle_payload_packet(msg);
            break;
         case ERROR_FILE_NOT_FOUND:
            handle_file_not_found(msg);
            break;
         case ERROR_CLIENT_VALIDATION_FAILED:
            handle_validation_failed(msg);
            break;
         case ERROR_CONNECTION_NOT_FOUND:
            handle_connection_not_found(msg);
            break;
         // Ignore unknown packets
         default:
            return;
      }
   }
   // ------------------------------------------------------------------------
   void Client::handle_validation_request(Message<ServerMsgType>& msg)
   {
      using namespace boost::multiprecision;

      auto end = NOW;

      uint32_t filenameSize = msg.header.size - SERVER_VALIDATION_REQUEST_META_DATA_SIZE;
      uint8_t difficulty;
      unsigned char hash1[SHA256_SIZE];
      unsigned char hash2[SHA256_SIZE];
      uint32_t nonce;
      std::string filename(filenameSize, '\0');

      msg >> filename;
      msg >> nonce;
      msg >> hash2;
      msg >> hash1;
      msg >> difficulty;

      auto duration = chrono::duration_cast<timeunit>(end - fileRequests.at(filename).tp);
      ++rttCount;
      rttCurrent = duration.count();
      rttTotal += rttCurrent;

      PLOG_INFO << "[Client] Got Validation Request for file: " << filename;

      // find a solution by converting to and from 256 wide ints
      unsigned char candidate[SHA256_SIZE];
      unsigned char candidate_hash[SHA256_SIZE];
      uint256_t bigint;
      import_bits(bigint, std::begin(hash1), std::end(hash1));
      for (uint256_t i = 0, one256 = 1; i < one256 << difficulty; ++i) {
         uint256_t tmp = bigint;
         tmp |= i;
         export_bits(tmp, std::begin(candidate), 8);
         compute_SHA256(candidate, SHA256_SIZE, candidate_hash);

         if (std::strncmp(reinterpret_cast<char*>(hash2), reinterpret_cast<char*>(candidate_hash), SHA256_SIZE) == 0) {
            // found a solution
            auto& fr = fileRequests.at(filename);
            std::memcpy(fr.hash1Solution, candidate, SHA256_SIZE);
            fr.nonce = nonce;
            break;
         }
      }

      Message<ClientMsgType> msgOut;
      msgOut.header.type = CLIENT_VALIDATION_RESPONSE;
      msgOut.header.size = 0;
      msgOut.header.remote = socket.local_endpoint();

      msgOut << CLIENT_VALIDATION_RESPONSE;
      msgOut << candidate;
      msgOut << nonce;
      msgOut << MAX_THROUGHPUT;
      msgOut << filename;

      fileRequests.at(filename).retryCounter = 1;
      set_timeout_for_validation_response(filename);
      fileRequests.at(filename).tp = NOW;

      send_msg(msgOut);
   }
   // ------------------------------------------------------------------------
   void Client::handle_initial_response(Message<ServerMsgType>& msg)
   {
      auto end = NOW;

      size_t filenameSize = msg.header.size - SERVER_INITIAL_RESPONSE_META_DATA;

      ConnectionID connectionId;
      uint64_t fileSize;
      unsigned char sha256[SHA256_SIZE];
      std::string filename(filenameSize, '\0');

      msg >> filename;
      msg >> sha256;
      msg >> fileSize;
      msg >> connectionId;

      auto duration = chrono::duration_cast<timeunit>(end - fileRequests.at(filename).tp);
      ++rttCount;
      rttCurrent = duration.count();
      rttTotal += rttCurrent;

      fileRequests.erase(filename);

      PLOG_INFO << "[Client] Got Initial response for file: " << filename;

      auto connectionResumption = std::find_if(connections.begin(), connections.end(), [&](auto& conn) {
         return std::filesystem::path(conn.second.filename).filename() == filename;
      });

      if (connectionResumption != connections.end()) {
         // is connection resumption
         auto& conn = connectionResumption->second;
         if (std::strncmp(reinterpret_cast<char*>(conn.sha256), reinterpret_cast<char*>(sha256), SHA256_SIZE) != 0) {
            // file changed
            conn.file.close();
            std::remove(conn.filename.c_str());
            connections.erase(connectionResumption->first);
         } else {
            // file not changed -> need to update connectionId key
            auto nh = connections.extract(connectionResumption->first);
            nh.key() = connectionId;
            connections.insert(std::move(nh));
            // since unordered_map::insert only inserts e if e is not present in map, the second insert call below won't insert the connection a second time
         }
      }

      std::string dest = fileDest + "/" + filename;
      Window window{MAX_THROUGHPUT * 1024 * 1024 / CHUNK_SIZE};

      try {
         connections.insert({connectionId, Connection{dest, fileSize, sha256, std::move(window), io_context}});
      } catch (const std::system_error& ex) {
         PLOG_ERROR << "[Client] Error when initializing Connection. ";
         done = connections.empty() && fileRequests.empty();
         send_finish_msg(connectionId);
         return;
      }

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
         auto duration = chrono::duration_cast<timeunit>(end - conn.tp);
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

      if (conn.window.isWindowComplete()) {
         // TODO: maybe extract this to separate function
         conn.t.cancel();

         uint32_t bytesWritten = 0;
         for (size_t i = 0; i < currentWindowSize; ++i) {
            uint32_t bytes = conn.window.chunks[i].size();
            conn.file.write(reinterpret_cast<char*>(conn.window.chunks[i].data()), bytes);

            // No space left
            if (!conn.file) {
               PLOG_WARNING << "[Client] Could not write to file " << conn.filename;
               send_finish_msg(connectionId);
               return;
            }

            bytesWritten += bytes;
         }
         conn.bytesWritten += bytesWritten;
         conn.chunksWritten += currentWindowSize;
         conn.file.flush();

         PLOG_VERBOSE << "[Client] Written " << currentWindowSize << " chunk" << ((currentWindowSize > 1) ? "s" : "")
                      << "(" << bytesWritten << "B)"
                      << " to disk";

         if (conn.isFileTransferComplete()) {
            unsigned char sha256[SHA256_SIZE];
            compute_file_SHA256(conn.filename, sha256);
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

      Message<ClientMsgType> msgOut;
      msgOut.header.type = TRANSMISSION_REQUEST;
      msgOut.header.size = 0;
      msgOut.header.remote = socket.local_endpoint();

      auto& conn = connections.at(connectionId);
      msgOut << TRANSMISSION_REQUEST;
      msgOut << connectionId;
      msgOut << conn.window.id;
      msgOut << rttCurrent;
      msgOut << conn.chunksWritten;

      conn.window.reset();

      PLOG_VERBOSE << "[Client] Requesting chunks at index: " << conn.chunksWritten << " for file " << conn.filename;

      conn.shouldMeasureTime = true;
      conn.tp = NOW;

      send_msg(msgOut);
   }
   // ------------------------------------------------------------------------
   void Client::request_retransmission(ConnectionID connectionId)
   {
      set_timeout_for_retransmission(connectionId);

      Message<ClientMsgType> msgOut;
      msgOut.header.type = RETRANSMISSION_REQUEST;
      msgOut.header.size = 0;
      msgOut.header.remote = socket.local_endpoint();

      auto& conn = connections.at(connectionId);

      Bitfield bitfield(conn.window.currentSize);
      bitfield.from(conn.window.sequenceNumbers);

      msgOut << RETRANSMISSION_REQUEST;
      msgOut << connectionId;
      msgOut << conn.window.id;
      msgOut << bitfield.bitfield;

      PLOG_INFO << "[Client] Requesting retransmission for connection ID " << connectionId;

      conn.shouldMeasureTime = true;
      conn.tp = NOW;

      send_msg(msgOut);
   }
   // ------------------------------------------------------------------------
   void Client::send_finish_msg(ConnectionID connectionId)
   {
      Message<ClientMsgType> msgOut;
      msgOut.header.type = CLIENT_FINISH_MESSAGE;
      msgOut.header.size = 0;
      msgOut.header.remote = socket.local_endpoint();

      msgOut << CLIENT_FINISH_MESSAGE;
      msgOut << connectionId;

      send_msg(msgOut);
   }
   // ------------------------------------------------------------------------
   // TODO: think about a Timer class that can register a timeout with a generic callback function
   // ------------------------------------------------------------------------
   void Client::set_timeout_for_file_request(std::string& filename)
   {
      auto& fr = fileRequests.at(filename);
      // Set a long timeout for a file request (calculation of SHA256 can take a while)
      fr.t.expires_after(minutes(10));
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
   void Client::set_timeout_for_validation_response(std::string& filename)
   {
      auto& fr = fileRequests.at(filename);
      // Set a reasonably long timeout to validate the solution
      fr.t.expires_after(minutes(1));
      fr.t.async_wait(boost::bind(&Client::handle_validation_response_timeout, this, filename));
   }
   // ------------------------------------------------------------------------
   void Client::handle_validation_response_timeout(std::string& filename)
   {
      auto search = fileRequests.find(filename);
      if (search != fileRequests.end()) {
         auto& fr = search->second;

         if (fr.retryCounter >= fr.maxRetries) {
            PLOG_ERROR << "[Client] Sent validation response for " << filename << " multiple times without success.";
            fileRequests.erase(filename);
            done = fileRequests.empty() && connections.empty();
            // wake the main thread that is waiting on the msqQueue condition variable
            msgQueue.wake();
            return;
         }

         if (fr.t.expiry() <= steady_timer::clock_type::now()) {
            PLOG_INFO << "[Client] Repeating validation response for file : " << filename;
            ++fr.retryCounter;

            Message<ClientMsgType> msgOut;
            msgOut.header.type = CLIENT_VALIDATION_RESPONSE;
            msgOut.header.size = 0;
            msgOut.header.remote = socket.local_endpoint();

            msgOut << CLIENT_VALIDATION_RESPONSE;
            msgOut << fr.hash1Solution;
            msgOut << fr.nonce;
            msgOut << MAX_THROUGHPUT;
            msgOut << filename;

            set_timeout_for_validation_response(filename);
            send_msg(msgOut);
         }
      }
   }
   // ------------------------------------------------------------------------
   void Client::handle_validation_failed(Message<ServerMsgType>& msg)
   {
      size_t filenameSize = msg.header.size - CLIENT_VALIDATION_FAILED_META_DATA;
      std::string filename(filenameSize, '\0');

      msg >> filename;

      fileRequests.erase(filename);
      done = connections.empty() && fileRequests.empty();

      PLOG_WARNING << "[Client] Validation failed for file " << filename << "\nYou might want to retry the file transfer!";
   }
   // ------------------------------------------------------------------------
   void Client::handle_file_not_found(Message<ServerMsgType>& msg)
   {
      size_t filenameSize = msg.header.size - FILE_NOT_FOUND_META_DATA;
      std::string filename(filenameSize, '\0');

      msg >> filename;

      fileRequests.erase(filename);
      done = connections.empty() && fileRequests.empty();

      PLOG_WARNING << "[Client] File " << filename << " not found on server!";
   }
   // ------------------------------------------------------------------------
   void Client::handle_connection_not_found(Message<ServerMsgType>& msg)
   {
      ConnectionID connectionId;

      msg >> connectionId;

      auto search = connections.find(connectionId);
      if (search != connections.end()) {
         auto& conn = search->second;
         request_file(std::filesystem::path(conn.filename).filename());
      }
   }
   // ------------------------------------------------------------------------
   void Client::set_timeout_for_transmission(ConnectionID connectionId)
   {
      auto& conn = connections.at(connectionId);
      conn.t.expires_after(timeunit(rttTotal / rttCount * TIMEOUT));
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
            std::remove(conn.filename.c_str());
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
      conn.t.expires_after(timeunit(rttTotal / rttCount * TIMEOUT));
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
            std::remove(conn.filename.c_str());
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
   void Client::handle_user_termination()
   {
      signal(SIGINT, [](int signum) {
         PLOG_WARNING << "[Client] Aborted file transfer. Deleting all incomplete files!";
         Client::abort = 1;
      });
   }
   // ------------------------------------------------------------------------
   void Client::delete_incomplete_files()
   {
      for (auto& conn: connections) {
         PLOG_WARNING << "[Client] Deleting incomplete file " << conn.second.filename;
         conn.second.file.close();
         std::remove(conn.second.filename.c_str());

         // Specification says that a Client Connection Termination message should be sent.
         // In hindsight, there is no need for that message as the Client Finish Message can be used instead (the server does not need to know why the client terminated/finished)
         send_finish_msg(conn.first);
      }
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
