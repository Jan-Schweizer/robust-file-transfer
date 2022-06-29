// ------------------------------------------------------------------------
#include "Client.hpp"
#include "util.hpp"
#include <boost/bind/bind.hpp>
#include <numeric>
#include <plog/Log.h>
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
      if (thread_context.joinable()) thread_context.join();
      io_context.stop();
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
      tmpMsgOut.header.size = sizeof(uint8_t) + sizeof(ConnectionID) + filename.size() + 1;// 1 byte for \0

      char* payload = tmpMsgOut.body;
      *payload = ClientMsgType::FILE_REQUEST;
      payload += sizeof(uint8_t);
      // Initial ConnectionId is set to 0
      *payload = 0;
      ++payload;
      *payload = 0;
      ++payload;
      std::memcpy(payload, filename.data(), filename.size() + 1);

      PLOG_INFO << "[Client] Requesting file: " << filename;
      send_msg(tmpMsgOut);
   }
   // ------------------------------------------------------------------------
   void Client::send_msg(Message<ClientMsgType> msg)
   {
      socket.async_send_to(buffer(msg.body, msg.header.size), server_endpoint,
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
      socket.async_receive_from(buffer(tmpMsgIn.body, PACKET_SIZE), remote_endpoint,
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
      auto& msg = tmpMsgIn.body;

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
      hexdump(msg.body, msg.header.size);

      size_t filenameSize = msg.header.size - (sizeof(ServerMsgType) + sizeof(ConnectionID) + sizeof(uint32_t) + SHA256_SIZE);

      ConnectionID connectionId;
      uint32_t fileSize;
      char sha256[SHA256_SIZE];
      std::string filename(filenameSize, '\0');

      msg >> STREAM_TYPE(char){*filename.data(), filenameSize};
      msg >> STREAM_TYPE(char){*sha256, SHA256_SIZE};
      msg >> STREAM_TYPE(uint32_t){fileSize, sizeof(uint32_t)};
      msg >> STREAM_TYPE(ConnectionID){connectionId, sizeof(ConnectionID)};

      PLOG_INFO << "[Client] Got Initial response for file: " << filename;

      std::string dest = fileDest + "/" + filename;
      fileTransfers.insert({connectionId, FileTransfer{dest, fileSize, sha256}});

      // TODO: Send first Transmission Request
   }
   // ------------------------------------------------------------------------
}// namespace rft
// ------------------------------------------------------------------------
