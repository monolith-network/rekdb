#ifndef REKDB_SERVER_HPP
#define REKDB_SERVER_HPP

#include <atomic>
#include <string>
#include <rocksdb/db.h>
#include <thread>
#include <httplib.h>

namespace rekdb {

class server_c {

public:
   server_c() = delete;
   server_c(const std::string& address, 
            const short port, 
            const std::string& db_location);
   ~server_c();

   bool start();
   bool stop();

private:
   std::string _address;
   short _port {0};
   std::string _db_location;
   rocksdb::DB* _db {nullptr};
   std::atomic<bool> _running;
   httplib::Server* _http_server {nullptr};
   std::thread _http_thread;

   void setup_endpoints();
   bool valid_req(const httplib::Request& req, httplib::Response& res);
   void probe(const httplib::Request& req ,httplib:: Response &res);
   void submit(const httplib::Request& req ,httplib:: Response &res);
   void fetch(const httplib::Request& req ,httplib:: Response &res);
   void remove(const httplib::Request& req ,httplib:: Response &res);
};

}

#endif