#ifndef REKDB_SERVER_HPP
#define REKDB_SERVER_HPP

#include <atomic>
#include <string>
#include <rocksdb/db.h>

namespace rekdb {

class server_c {

public:
   server_c() = delete;
   server_c(const short port, const std::string& db_location);

   bool start();
   bool stop();

private:
   short _port {0};
   std::string _db_location;
   rocksdb::DB* _db {nullptr};
   std::atomic<bool> _running;
};

}

#endif