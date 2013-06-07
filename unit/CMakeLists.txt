include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(gtest STATIC gtest-all.cc)
add_executable(gtest_main gtest_main.cc acceptor_unittest.cc
	learner_unittest.cc proposer_unittest.cc)
# On Fedora and possibly other platforms, pthread is not linked implicitly 
# (needed by gtest)
target_link_libraries(gtest_main paxos gtest pthread)
add_test(gtest_main gtest_main)
