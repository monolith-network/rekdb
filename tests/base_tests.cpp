#include <iostream>
#include <libutil/random/generator.hpp>

#include <iostream>
#include <limits>
#include <stdint.h>

#include <chrono>
#include <thread>

// This has to be included last as there is a known issue
// described here: https://github.com/cpputest/cpputest/issues/982
//
#include "CppUTest/TestHarness.h"

TEST_GROUP(base_test){void setup(){} void teardown(){}};

TEST(base_test, basic_test)
{
   /*
   auto value =
         libutil::random::generate_random_c<double>().get_floating_point_range(
               std::numeric_limits<double>::min(),
               std::numeric_limits<double>::max());
   std::cout << "Value: " << value << std::endl;
   orb::setup_logger(AixLog::Severity::debug);
   orb::server::config config{"test_node", 5000, nettle::HostPort("127.0.0.1", 4096)};
   orb::server server(config, nullptr);
   CHECK_FALSE(server.add_endpoint(nullptr));
   */
/*
   using namespace std::chrono_literals;
   CHECK_TRUE(server.start());
   std::this_thread::sleep_for(250ms);
   CHECK_TRUE(server.stop());
   */
}