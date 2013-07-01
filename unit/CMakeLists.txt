include_directories(${CMAKE_SOURCE_DIR}/paxos/include)
include_directories(${GTEST_INCLUDE_DIRS})

add_executable(gtest gtest.cc acceptor_unittest.cc learner_unittest.cc 
	proposer_unittest.cc)

target_link_libraries(gtest ${GTEST_LIBRARIES} paxos)
add_test(gtest gtest)
