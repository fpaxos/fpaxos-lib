include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})
include_directories(${BDB_INCLUDE_DIRS})

SET(LOCAL_SOURCES acceptor_state.c acceptor.c carray.c config_reader.c
	learner_state.c learner.c proposer_state.c proposer.c storage.c
	tcp_receiver.c tcp_sendbuf.c)

add_library(paxos_shared SHARED ${LOCAL_SOURCES} )
add_library(paxos STATIC ${LOCAL_SOURCES} )

set_target_properties(paxos_shared PROPERTIES OUTPUT_NAME paxos)

target_link_libraries(paxos_shared ${LIBPAXOS_LINKER_LIBS} ${BDB_LIBRARIES} 
	${LIBEVENT_LIBRARIES})
target_link_libraries(paxos ${LIBPAXOS_LINKER_LIBS} ${BDB_LIBRARIES}
	${LIBEVENT_LIBRARIES})
