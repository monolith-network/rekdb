#include "server.hpp"
#include <externals/aixlog/logger.hpp>
#include <externals/simplejson/json.hpp>
#include <functional>
#include <chrono>

using namespace std::chrono_literals;

namespace rekdb {

namespace {
   enum class return_codes_e {
      OKAY = 200,
      BAD_REQUEST_400 = 400,
      INTERNAL_SERVER_500 = 500,
      NOT_IMPLEMENTED_501 = 501,
      GATEWAY_TIMEOUT_504 = 504
   };

   inline std::string get_json_response(const return_codes_e rc, 
                                       const std::string msg) {
   return "{\"status\":" + 
      std::to_string(static_cast<uint32_t>(rc)) +  
      ",\"data\":\"" +
       msg + 
       "\"}";
   }

   inline std::string get_json_raw_response(const return_codes_e rc, 
                                          const std::string json) {
      return "{\"status\":" + 
         std::to_string(static_cast<uint32_t>(rc)) +  
         ",\"data\":" +
         json + 
         "}";
   }
}

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
      LOG(ERROR) << TAG("server_c::start") << "Unable to open database file : " << _db_location << "\n";
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

void server_c::setup_endpoints() {

   _http_server->Get(R"(/probe/(.*?))", 
      std::bind(&server_c::probe, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));
                  
   _http_server->Get(R"(/submit/(.*?))", 
      std::bind(&server_c::submit, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));
                  
   _http_server->Get(R"(/fetch/(.*?))", 
      std::bind(&server_c::fetch, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));
                  
   _http_server->Get(R"(/delete/(.*?))", 
      std::bind(&server_c::remove, 
                  this, 
                  std::placeholders::_1, 
                  std::placeholders::_2));
}

bool server_c::valid_req(const httplib::Request& req, httplib::Response& res) {
   if (req.matches.size() < 2) {
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Json data not detected"), 
         "text/json"
         );
      return false;
   }
   return true;
}

void server_c::probe(const httplib::Request& req, httplib::Response& res) {
   if (!valid_req(req, res)) { return; }
   
   auto endpoint = req.matches[0];
   auto json_data = std::string(req.matches[1]);
   LOG(DEBUG) << TAG("server_c::probe") << "Got data: " << json_data << "\n";

   json::jobject result;
   if (!json::jobject::tryparse(json_data.c_str(), result)) {
      LOG(ERROR) << TAG("server_c::probe")
                  << endpoint.str()
                  << " : Unparsable message received : \n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Unparseable request"), 
         "text/json"
         );
      return;
   }

   // Key
   //
   if (!result.has_key("key")) {
      LOG(ERROR) << TAG("server_c::probe")
                  << endpoint.str()
                  << " : Message does not contain a key\n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Request omits key"), 
         "text/json"
         );
      return;
   }
   std::string key = result.get("key");
   key = key.substr(1, key.length()-2);   // Trim quotes

   LOG(DEBUG) << TAG("server_c::probe") << "Got key: " << key << "\n";

   std::string item_value;

   // Attempt to get the key 
   //
   rocksdb::Status status = _db->Get(rocksdb::ReadOptions(), key, &item_value);

   if (status.IsNotFound()) {
      res.set_content(
         get_json_response(
            return_codes_e::OKAY,
            "not found"), 
         "text/json"
         );
      return;
   }

   if (status.ok()) {
      res.set_content(
         get_json_response(
            return_codes_e::OKAY,
            "found"), 
         "text/json"
         );
      return;
   }

   // Internal error
   //
   res.set_content(
      get_json_response(
         return_codes_e::INTERNAL_SERVER_500,
         "server error"), 
      "text/json"
      );
}

void server_c::submit(const httplib::Request& req, httplib::Response& res) {
   if (!valid_req(req, res)) { return; }
   
   auto endpoint = req.matches[0];
   auto json_data = std::string(req.matches[1]);
   LOG(DEBUG) << TAG("server_c::submit") << "Got data: " << json_data << "\n";

   json::jobject result;
   if (!json::jobject::tryparse(json_data.c_str(), result)) {
      LOG(ERROR) << TAG("server_c::submit")
                  << endpoint.str()
                  << " : Unparsable message received : \n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Unparseable request"), 
         "text/json"
         );
      return;
   }

   // Key
   //
   if (!result.has_key("key")) {
      LOG(ERROR) << TAG("server_c::submit")
                  << endpoint.str()
                  << " : Message does not contain a key\n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Request omits key"), 
         "text/json"
         );
      return;
   }
   std::string key = result.get("key");
   key = key.substr(1, key.length()-2);   // Trim quotes

   LOG(DEBUG) << TAG("server_c::submit") << "Got key: " << key << "\n";

   // Value
   if (!result.has_key("value")) {
      LOG(ERROR) << TAG("server_c::submit")
                  << endpoint.str()
                  << " : Message does not contain a value\n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Request omits value"), 
         "text/json"
         );
      return;
   }
   std::string value = result.get("value");
   value = value.substr(1, value.length()-2);   // Trim quotes

   LOG(DEBUG) << TAG("server_c::submit") << "Got value: " << value << "\n";

   rocksdb::Status status = _db->Put(rocksdb::WriteOptions(), key, value);

   if (status.ok()) {
      res.set_content(
         get_json_response(
            return_codes_e::OKAY,
            "success"), 
         "text/json"
         );
      return;
   }

   // Internal error
   //
   res.set_content(
      get_json_response(
         return_codes_e::INTERNAL_SERVER_500,
         "server error"), 
      "text/json"
      );
}

void server_c::fetch(const httplib::Request& req, httplib::Response& res) {
   if (!valid_req(req, res)) { return; }

   auto endpoint = req.matches[0];
   auto json_data = std::string(req.matches[1]);
   LOG(DEBUG) << TAG("server_c::fetch") << "Got data: " << json_data << "\n";

   json::jobject result;
   if (!json::jobject::tryparse(json_data.c_str(), result)) {
      LOG(ERROR) << TAG("server_c::fetch")
                  << endpoint.str()
                  << " : Unparsable message received : \n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Unparseable request"), 
         "text/json"
         );
      return;
   }

   // Key
   //
   if (!result.has_key("key")) {
      LOG(ERROR) << TAG("server_c::fetch")
                  << endpoint.str()
                  << " : Message does not contain a key\n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Request omits key"), 
         "text/json"
         );
      return;
   }
   std::string key = result.get("key");
   key = key.substr(1, key.length()-2);   // Trim quotes

   LOG(DEBUG) << TAG("server_c::fetch") << "Got key: " << key << "\n";

   std::string item_value;

   // Attempt to get the key 
   //
   rocksdb::Status status = _db->Get(rocksdb::ReadOptions(), key, &item_value);

   if (status.IsNotFound()) {
      res.set_content(
         get_json_response(
            return_codes_e::OKAY,
            ""), 
         "text/json"
         );
      return;
   }

   if (status.ok()) {
      res.set_content(
         get_json_response(
            return_codes_e::OKAY,
            item_value), 
         "text/json"
         );
      return;
   }

   // Internal error
   //
   res.set_content(
      get_json_response(
         return_codes_e::INTERNAL_SERVER_500,
         "server error"), 
      "text/json"
      );
}

void server_c::remove(const httplib::Request& req, httplib::Response& res) {
   if (!valid_req(req, res)) { return; }

   auto endpoint = req.matches[0];
   auto json_data = std::string(req.matches[1]);
   LOG(DEBUG) << TAG("server_c::remove") << "Got data: " << json_data << "\n";

   json::jobject result;
   if (!json::jobject::tryparse(json_data.c_str(), result)) {
      LOG(ERROR) << TAG("server_c::remove")
                  << endpoint.str()
                  << " : Unparsable message received : \n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Unparseable request"), 
         "text/json"
         );
      return;
   }

   // Key
   //
   if (!result.has_key("key")) {
      LOG(ERROR) << TAG("server_c::remove")
                  << endpoint.str()
                  << " : Message does not contain a key\n"
                  << json_data << "\n";
      res.set_content(
         get_json_response(
            return_codes_e::BAD_REQUEST_400,
            "Request omits key"), 
         "text/json"
         );
      return;
   }
   std::string key = result.get("key");
   key = key.substr(1, key.length()-2);   // Trim quotes

   LOG(DEBUG) << TAG("server_c::remove") << "Got key: " << key << "\n";

   rocksdb::Status status = _db->Delete(rocksdb::WriteOptions(), key);

   if (status.ok()) {
      res.set_content(
         get_json_response(
            return_codes_e::OKAY,
            "okay"), 
         "text/json"
         );
      return;
   }

   // Internal error
   //
   res.set_content(
      get_json_response(
         return_codes_e::INTERNAL_SERVER_500,
         "server error"), 
      "text/json"
      );
}

}