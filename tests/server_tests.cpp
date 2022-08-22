#include <iostream>
#include <libutil/random/string.hpp>
#include <stdint.h>
#include <vector>
#include <string>

#include "server.hpp"
#include "externals/aixlog/logger.hpp"
#include "externals/simplejson/json.hpp"
#include <filesystem>

// This has to be included last as there is a known issue
// described here: https://github.com/cpputest/cpputest/issues/982
//
#include "CppUTest/TestHarness.h"

namespace {

   rekdb::server_c* server {nullptr};

   static constexpr char LOGS[] = "rekdb-tests.log";
   static constexpr char DB[] = "rekdb-tests.db";
   static constexpr char ADDR[] = "0.0.0.0";
   static constexpr short PORT = 20009;
   static constexpr char NOT_FOUND[] = "{\"status\":200,\"data\":\"not found\"}";
   static constexpr char FOUND[] = "{\"status\":200,\"data\":\"found\"}";
   static constexpr char SUCCESS[] = "{\"status\":200,\"data\":\"success\"}";

   static constexpr char DATA_POOL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "abcdefghijklmnopqrstuvwxyz";

   struct key_value_pair {
      std::string key;
      std::string value;
   };

   // Generate random key value pairs
   std::vector<key_value_pair> generate_data(size_t count) {

      std::vector<key_value_pair> data;
      data.reserve(count);

      for(decltype(count) i = 0; i < count; i++) {
         data.push_back({
            .key = libutil::random::random_string_c(DATA_POOL).generate_string(25),
            .value = libutil::random::random_string_c(DATA_POOL).generate_string(100)
         });
      }
      return data;
   }

   void setup_logger(AixLog::Severity level) {
      auto sink_cout =
            std::make_shared<AixLog::SinkCout>(level, "[#severity] (#tag) #message");
      auto sink_file = std::make_shared<AixLog::SinkFile>(
            level, LOGS,
            "%Y-%m-%d %H-%M-%S.#ms | [#severity] (#tag) #message");
      AixLog::Log::init({sink_cout, sink_file});
   }
}

TEST_GROUP(server_test)
{
   void setup() {
      setup_logger(AixLog::Severity::trace);

      server = new rekdb::server_c(
         ADDR, 
         PORT, 
         DB);

      if (!server->start()) {
         std::cerr << "Failed to start server with settings: "
                     << ADDR
                     << ":"
                     << PORT
                     << " with db: "
                     << DB
                     << std::endl;
         std::exit(1);
      }
   }

   void teardown() {
      server->stop();
      delete server;

      if (std::filesystem::is_directory(DB)) {
         if (!std::filesystem::remove_all(DB)) {
            std::cerr << "Unable to remove database file " 
                        << DB 
                        << " on test teardown" 
                        << std::endl;
            std::exit(1);
         }
      }
   }
};

TEST(server_test, submit_fetch_probe_delete)
{
   auto data = generate_data(100);

   httplib::Client cli(ADDR, PORT);

   // Ensure that the database doesn't contain any of the data generated
   for(auto item : data) {
      auto res = cli.Get("/probe/" + item.key);
      CHECK_TRUE_TEXT(res, "Failed to obtain result from probe");
      CHECK_EQUAL_TEXT(NOT_FOUND, res->body, "Item found when it shouldn't exist");
   }
   
   // Submit the data and ensure that it exists after submission
   for(auto item : data) {
      {
         auto res = cli.Get("/submit/" + item.key + "/" + item.value);
         CHECK_TRUE_TEXT(res, "Failed to obtain result from submit");
         CHECK_EQUAL_TEXT(SUCCESS, res->body, "Item failed to be inserted");
      }
      {
         auto res = cli.Get("/probe/" + item.key);
         CHECK_TRUE_TEXT(res, "Failed to obtain result from probe");
         CHECK_EQUAL_TEXT(FOUND, res->body, "Item not found when it shouldn't exist");
      }
   }

   // Retrieve the data
   for(auto item : data) {
      auto res = cli.Get("/fetch/" + item.key);
      CHECK_TRUE_TEXT(res, "Failed to obtain result from probe");
      CHECK_EQUAL_TEXT(item.value, res->body, "Expected data did not match actual");
   }

   // Delete some data
   for(auto item : data) {
      auto res = cli.Get("/delete/" + item.key);
      CHECK_TRUE_TEXT(res, "Failed to obtain result from probe");
      CHECK_EQUAL_TEXT(SUCCESS, res->body, "Unable to delete data");
   }

   // Ensure that the database doesn't contain any of the data generated
   for(auto item : data) {
      auto res = cli.Get("/probe/" + item.key);
      CHECK_TRUE_TEXT(res, "Failed to obtain result from probe");
      CHECK_EQUAL_TEXT(NOT_FOUND, res->body, "Item found when it shouldn't exist");
   }
}