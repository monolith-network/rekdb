#include <iostream>
#include <string>

#include <atomic>
#include <csignal>
#include <optional>
#include <toml++/toml.h>
#include <vector>
#include <chrono>
#include <thread>

#include "server.hpp"
#include <crate/externals/aixlog/logger.hpp>

using namespace std::chrono_literals;

namespace {
   struct configuration {
      short port {4096};
      std::string database_location;
      std::string address;
   };

   rekdb::server_c* server {nullptr};
   std::atomic<bool> active {true};
   std::atomic<bool> handling_signal {false};
}

void setup_logger(AixLog::Severity level) {

  auto sink_cout =
      std::make_shared<AixLog::SinkCout>(level, "[#severity] (#tag) #message");
  auto sink_file = std::make_shared<AixLog::SinkFile>(
      level, "rekdb.log",
      "%Y-%m-%d %H-%M-%S.#ms | [#severity] (#tag) #message");
  AixLog::Log::init({sink_cout, sink_file});
}

void show_usage() {
   std::cout << 
   "( -h | --help )            Show usage message\n"
   "( -c | --cfg  ) <file>     Launch rekdb with a config file\n"
   << std::endl;
}

void handle_signal(int signal) {

   if (handling_signal.load()) {
      return;
   }

   handling_signal.store(true);
   active.store(false);

   std::cout << "\nExiting" << std::endl;
}

void run(const configuration& cfg) {

   server = new rekdb::server_c(cfg.address, cfg.port, cfg.database_location);

   if (!server->start()) {
      std::cerr << "Unable to start server" << std::endl;
      std::exit(1);
   }

   while (active.load()) {
      std::this_thread::sleep_for(500ms);
   }

   server->stop();
   delete server;

   std::exit(0);
}

void execute_database(const std::string& file) {

   toml::table tbl;
   try {
      tbl = toml::parse_file(file);
   } catch (const toml::parse_error& err) {
      LOG(ERROR) << TAG("execute_database") 
         << "Unable to parse file : " 
         << *err.source().path 
         << ". Description: " 
         << err.description() 
         << " (" << err.source().begin << ")\n";
      std::exit(1);
   }

   configuration cfg;
   
   std::optional<short> port = 
      tbl["rekdb"]["port"].value<short>();
   if (port.has_value()) {
      cfg.port = *port;
   } else {
      LOG(ERROR) << TAG("load_config") << "Missing config for 'rekdb port'\n";
      std::exit(1);
   }

   std::optional<std::string> database_location = 
      tbl["rekdb"]["database_location"].value<std::string>();
   if (database_location.has_value()) {
      cfg.database_location = *database_location;
   } else {
      LOG(ERROR) << TAG("load_config") << "Missing config for 'rekdb database_location'\n";
      std::exit(1);
   } 
   
   std::optional<std::string> address = 
      tbl["rekdb"]["address"].value<std::string>();
   if (address.has_value()) {
      cfg.address = *address;
   } else {
      LOG(ERROR) << TAG("load_config") << "Missing config for 'rekdb address'\n";
      std::exit(1);
   } 

   return run(cfg);
}

int main(int argc, char** argv) {

   setup_logger(AixLog::Severity::debug);

   signal(SIGHUP , handle_signal);   /* Hangup the process */ 
   signal(SIGINT , handle_signal);   /* Interrupt the process */ 
   signal(SIGQUIT, handle_signal);   /* Quit the process */ 
   signal(SIGILL , handle_signal);   /* Illegal instruction. */ 
   signal(SIGTRAP, handle_signal);   /* Trace trap. */ 
   signal(SIGABRT, handle_signal);   /* Abort. */

   std::vector<std::string> args(argv, argv + argc);

   for(size_t i = 0; i < args.size(); i++) {
      //    Show usage
      //
      if (args[i] == "-h" || args[i] == "--help") {
         show_usage();
         return 0;
      }

      //    Launch app with config
      //
      if (args[i] == "-c" || args[i] == "--cfg") {
         if (i+1 >= args.size()) {
            std::cerr << "No configuration file given to arg \"" << args[i] << std::endl;
            std::exit(1);
         }
         ++i;

         execute_database(args[i]);
      }
   }

   std::cerr << "No arguments given. Use -h for help" << std::endl;

   return 1;
}