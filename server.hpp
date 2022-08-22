#ifndef REKDB_SERVER_HPP
#define REKDB_SERVER_HPP

#include <atomic>
#include <string>
#include <rocksdb/db.h>
#include <thread>
#include <httplib.h>
#include <tuple>

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
   enum class return_codes_e {
      OKAY = 200,
      BAD_REQUEST_400 = 400,
      INTERNAL_SERVER_500 = 500,
      NOT_IMPLEMENTED_501 = 501,
      GATEWAY_TIMEOUT_504 = 504
   };

   std::string _address;
   short _port {0};
   std::string _db_location;
   rocksdb::DB* _db {nullptr};
   std::atomic<bool> _running;
   httplib::Server* _http_server {nullptr};
   std::thread _http_thread;

   std::string get_json_response(const return_codes_e rc, 
                                    const std::string msg);
   std::string get_json_raw_response(const return_codes_e rc, 
                                          const std::string json);
   void setup_endpoints();
   bool valid_http_req(const httplib::Request& req, 
                        httplib::Response& res, 
                        size_t expected_items);
   void http_probe(const httplib::Request& req ,httplib:: Response &res);
   void http_submit(const httplib::Request& req ,httplib:: Response &res);
   void http_fetch(const httplib::Request& req ,httplib:: Response &res);
   void http_remove(const httplib::Request& req ,httplib:: Response &res);

   std::string run_probe(const std::string& key);
   std::string run_submit(const std::string& key, const std::string& value);
   std::tuple<std::string,
      std::string> run_fetch(const std::string& key);
   std::string run_remove(const std::string& key);
};

}

#endif