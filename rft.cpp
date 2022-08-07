#include "Client.hpp"
#include "Server.hpp"
#include <boost/program_options.hpp>
#include <iostream>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>

namespace po = boost::program_options;
using namespace std;

int main(int argc, char* argv[])
{
   // plog config
   static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
   plog::init(plog::info, &consoleAppender);


   string host;
   size_t port;
   double p;
   double q;
   string dest;
   vector<string> files;
   bool is_server = false;
   bool is_client = false;

   try {
      po::options_description desc{"Usage"};
      // clang-format off
      desc.add_options()
         ("help,h", "produce help message")
         ("s", "operate in server mode")
         ("host", po::value(&host),"the hostname to request from (hostname or IPv4 address)")
         ("t", po::value(&port)->default_value(8080), "the port number to use")
         ("p", po::value(&p), "packet loss probability")
         ("q", po::value(&q), "packets remain lost probability")
         ("files", po::value<vector<string>>()->multitoken(), "files to transfer")
         ("dest", po::value(&dest)->default_value("/tmp"), "the destination of the transferred files");
      // clang-format on

      po::positional_options_description positionals;
      positionals.add("host", 1);
      positionals.add("files", -1);

      po::variables_map vm;
      int style = po::command_line_style::allow_short |
                  po::command_line_style::short_allow_adjacent |
                  po::command_line_style::short_allow_next |
                  po::command_line_style::allow_long |
                  po::command_line_style::long_allow_adjacent |
                  po::command_line_style::long_allow_next |
                  po::command_line_style::allow_dash_for_short |
                  po::command_line_style::allow_long_disguise;
      po::store(po::command_line_parser(argc, argv)
                    .options(desc)
                    .positional(positionals)
                    .style(style)
                    .run(),
                vm);
      po::notify(vm);

      if (vm.count("help")) {
         cout << desc << endl;
         return 0;
      }

      if (vm.count("s") && vm.count("host")) {
         throw std::logic_error{"Cannot be server and host at the same time"};
      }

      if (vm.count("s")) {
         if (vm.count("files")) {
            throw std::logic_error{"Cannot specify files in server mode"};
         }
         is_server = true;
         cout << "Server mode" << endl;
      }

      if (vm.count("host")) {
         if (!vm.count("files")) {
            throw std::logic_error{"Must specify files in client mode"};
         }
         cout << "Files to transfer: ";
         files = vm["files"].as<vector<string>>();
         for (const auto& file: files) {
            cout << file << ", ";
         }
         cout << endl;

         is_client = true;
         cout << "Client mode with host: " << vm["host"].as<string>() << endl;
      }

      cout << "Port to use: " << port << endl;

      if (vm.count("p") && !vm.count("q")) {
         q = vm["p"].as<double>();
      }

      if (vm.count("q") && !vm.count("p")) {
         p = vm["q"].as<double>();
      }

      if (!vm.count("p") && !vm.count("q")) {
         p = 0;
         q = 1;
      }

      cout << "p: " << p << "\tq: " << q << endl;

      cout << "File destination: " << dest << endl;

   } catch (const exception& ex) {
      cerr << ex.what() << endl;
   }

   if (is_server) {
      try {
         rft::Server server(port);
         server.start();
      } catch (std::exception& e) {
         PLOG_ERROR << e.what();
      }
   } else if (is_client) {
      try {
         rft::Client client(host, port, dest);
         client.request_files(files);
      } catch (std::exception& e) {
         PLOG_ERROR << e.what();
      }
   } else {
      PLOG_ERROR << "ERROR: Program neither run in server nor in client mode!";
   }

   return 0;
}
