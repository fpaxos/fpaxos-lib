include_directories(${CMAKE_SOURCE_DIR}/paxos/include)

add_subdirectory(gtest)

add_executable(runtest runtest.cc acceptor_unittest.cc learner_unittest.cc proposer_unittest.cc)
target_link_libraries(runtest paxos pthread gtest-all)
add_test(unit-tests runtest)
 