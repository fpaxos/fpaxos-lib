include_directories(${CMAKE_SOURCE_DIR}/util)
include_directories(${CMAKE_SOURCE_DIR}/db)
include_directories(${CMAKE_SOURCE_DIR}/libtapioca)
include_directories(${LIBEVENT_INCLUDE_DIRS})
include_directories(${LIBPAXOS_INCLUDE_DIRS})

add_library(gtest STATIC gtest-all.cc)
add_executable(gtest_main gtest_main.cc acceptor_unittest.cc)

target_link_libraries(gtest_main gtest paxos)

add_test(gtest_main gtest_main)
