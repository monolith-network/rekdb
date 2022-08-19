#include "server.hpp"

#include <externals/aixlog/logger.hpp>

namespace rekdb {


server_c::server_c(const short port, const std::string& db_location) : 
   _port(port),
   _db_location(db_location) {

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

   _running.store(true);

   // Start whatever threads are required

   return true;
}

bool server_c::stop() {
   if (!_running.load()) {
      return true;
   }


   return true;
}

}