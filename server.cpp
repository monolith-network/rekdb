#include "server.hpp"
#include <externals/aixlog/logger.hpp>
#include <functional>
#include <chrono>

using namespace std::chrono_literals;

namespace rekdb {

server_c::server_c(const std::string& address, 
                   const short port, 
                   const std::string& db_location) : 
   _address(address),
   _port(port),
   _db_location(db_location) {

   _http_server = new httplib::Server();

   LOG(DEBUG) << TAG("server_c::server_c") 
               << "Server created with port: " 
               << port 
               << ", and database: " 
               << db_location 
               << "\n";
               
   _http_server->set_logger([](const httplib::Request &req, const httplib::Response &res) {

      // req.matches[0] should always be present but a
      // segfault was caught twice coming from
      // accessing matches[0]
      std::string endpoint{};
      if (!req.matches.empty()) {
         endpoint = ", endpoint:" + req.matches[0].str();
      }

      LOG(DEBUG) << TAG("httplib") 
                  << "[address:" << req.remote_addr 
                  << ", port:" << req.remote_port
                  << ", agent:" << req.get_header_value("User-Agent")
                  << endpoint
                  << ", method:" << req.method
                  << ", body:" << req.body
                  << "]\n";
   });
}

server_c::~server_c() {

   if (_http_server->is_running()) {
      _http_server->stop();
   }

   if (_http_thread.joinable()) {
      _http_thread.join();
   }

   delete _http_server;

   _db->Close();
}

bool server_c::start() {
   if (_running.load()) {
      return true;
   }
   
   rocksdb::Options options;
   options.create_if_missing = true;
   rocksdb::Status status = rocksdb::DB::Open(options, _db_location, &_db);

   if (!status.ok()) {
      LOG(ERROR) << TAG("server_c::start") 
                  << "Unable to open database file : " 
                  << _db_location 
                  << "\n";
      return false;
   }

   setup_endpoints();

   auto http_runner = [](httplib::Server* server, const std::string address, const short port) {
      server->listen(address, port);
   };

   _http_thread = std::thread(http_runner, _http_server, _address, _port);

   std::this_thread::sleep_for(10ms);

   if (!_http_server->is_running()) {
      LOG(ERROR) << TAG("server_c::start") << "Failed to start http server\n";
      return false;
   }

   _running.store(true);
   return true;
}

bool server_c::stop() {
   if (!_running.load()) {
      return true;
   }

   _http_server->stop();

   if (_http_thread.joinable()) {
      _http_thread.join();
   }

   _db->Close();
   _running.store(false);
   return true;
}

std::string server_c::get_json_response(const return_codes_e rc, const std::string msg) {
   return "{\"status\":" + 
      std::to_string(static_cast<uint32_t>(rc)) +
      ",\"data\":\"" +
         msg + 
         "\"}";
}

std::string server_c::get_json_raw_response(const return_codes_e rc, const std::string json) {
   return "{\"status\":" + 
      std::to_string(static_cast<uint32_t>(rc)) +  
      ",\"data\":" +
      json + 
      "}";
}

void server_c::setup_endpoints() {

   _http_server->Get("/", 
      std::bind(&server_c::http_root, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));

   _http_server->Get(R"(/probe/(.*?))", 
      std::bind(&server_c::http_probe, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));

   _http_server->Get(R"(/submit/(.*?)/(.*?))", 
      std::bind(&server_c::http_submit, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));

   _http_server->Get(R"(/fetch/(.*?))", 
      std::bind(&server_c::http_fetch, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));

   _http_server->Get(R"(/delete/(.*?))", 
      std::bind(&server_c::http_remove, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));
}

bool server_c::valid_http_req(const httplib::Request& req, httplib::Response& res, 
                              size_t expected_items) {
   if (req.matches.size() < expected_items) {
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Json data not detected"), 
         "application/json"
         );
      return false;
   }
   return true;
}

void server_c::http_root(const httplib::Request& req ,httplib:: Response &res) {
   res.set_content(
      get_json_response(
         return_codes_e::OKAY,
         "success"),
       "application/json");
}

void server_c::http_probe(const httplib::Request& req, httplib::Response& res) {
   if (!valid_http_req(req, res, 2)) { return; }
   
   auto endpoint = req.matches[0];
   auto key = std::string(req.matches[1]);

   LOG(DEBUG) << TAG("server_c::http_probe") << "Got key: " << key << "\n";

   res.set_content(run_probe(key), "application/json");
}

void server_c::http_submit(const httplib::Request& req, httplib::Response& res) {
   if (!valid_http_req(req, res, 3)) { return; }
   
   auto endpoint = req.matches[0];
   auto key = std::string(req.matches[1]);
   auto value = std::string(req.matches[2]);

   LOG(DEBUG) << TAG("server_c::http_submit") << "Got key: " << key << "\n";
   LOG(DEBUG) << TAG("server_c::http_submit") << "Got value: " << value << "\n";

   res.set_content(run_submit(key, value), "application/json");
}

void server_c::http_fetch(const httplib::Request& req, httplib::Response& res) {
   if (!valid_http_req(req, res, 2)) { return; }

   auto endpoint = req.matches[0];
   auto key = std::string(req.matches[1]);
   LOG(DEBUG) << TAG("server_c::http_fetch") << "Got key: " << key << "\n";

   auto [response, response_type] = run_fetch(key);

   res.set_content(response, response_type);
}

void server_c::http_remove(const httplib::Request& req, httplib::Response& res) {
   if (!valid_http_req(req, res, 2)) { return; }

   auto endpoint = req.matches[0];
   auto key = std::string(req.matches[1]);
   LOG(DEBUG) << TAG("server_c::http_remove") << "Got key: " << key << "\n";

   rocksdb::Status status = _db->Delete(rocksdb::WriteOptions(), key);

   if (status.ok()) {
      res.set_content(
         get_json_response(
            return_codes_e::OKAY,
            "success"), 
         "application/json"
         );
      return;
   }

   // Internal error
   //
   res.set_content(
      get_json_response(
         return_codes_e::INTERNAL_SERVER_500,
         "server error"), 
      "application/json"
      );
}

std::string server_c::run_probe(const std::string& key) {

   std::string item_value;
   rocksdb::Status status = _db->Get(rocksdb::ReadOptions(), key, &item_value);
   
   if (status.IsNotFound()) {
      return get_json_response(return_codes_e::OKAY, "not found");
   }

   if (status.ok()) {
      return get_json_response(return_codes_e::OKAY, "found");
   }

   return get_json_response(return_codes_e::INTERNAL_SERVER_500, "server error");
}

std::string server_c::run_submit(const std::string& key, const std::string& value) {

   rocksdb::Status status = _db->Put(rocksdb::WriteOptions(), key, value);

   if (status.ok()) {
      return get_json_response(return_codes_e::OKAY, "success");
   }

   return get_json_response(return_codes_e::INTERNAL_SERVER_500, "server error");
}

std::tuple<std::string,
      std::string> server_c::run_fetch(const std::string& key) {

   std::string item_value;
   rocksdb::Status status = _db->Get(rocksdb::ReadOptions(), key, &item_value);
   
   if (status.IsNotFound()) {
      return {get_json_response(return_codes_e::OKAY, "not found"), "application/json"};
   }

   if (status.ok()) {
      return {item_value, "text/plain"};
   }

   return {
      get_json_response(return_codes_e::INTERNAL_SERVER_500, "server error"),
      "application/json"};
}

std::string server_c::run_remove(const std::string& key){

   rocksdb::Status status = _db->Delete(rocksdb::WriteOptions(), key);

   if (status.ok()) {
      return get_json_response(
            return_codes_e::OKAY,
            "success");
   }

   return get_json_response(return_codes_e::INTERNAL_SERVER_500, "server error");
}

} // namespace rekdb 