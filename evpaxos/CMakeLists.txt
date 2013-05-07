include_directories(include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

SET(LOCAL_SOURCES acceptor.c config_reader.c learner.c proposer.c
	tcp_receiver.c tcp_sendbuf.c)

add_library(evpaxos SHARED ${LOCAL_SOURCES})

target_link_libraries(evpaxos paxos ${LIBPAXOS_LINKER_LIBS} 
	${LIBEVENT_LIBRARIES})




