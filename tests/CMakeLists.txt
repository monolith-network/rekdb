link_directories(${CPPUTEST_LIBRARIES})

find_package(libutil REQUIRED)

include_directories(
  ${CPPUTEST_INCLUDE_DIRS}
  ${LIBUTIL_INCLUDE_DIRS}
)

add_executable(rekdb-tests
        ${SOURCES}
        base_tests.cpp
        main.cpp)


target_link_libraries(rekdb-tests
        ${CPPUTEST_LDFLAGS}
        libutil)

add_custom_command(TARGET rekdb-tests COMMAND ./rekdb-tests POST_BUILD)
