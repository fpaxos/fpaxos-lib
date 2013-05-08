include_directories(include)
include_directories(${LIBEVENT_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/include)

SET(LOCAL_SOURCES config_reader.c tcp_receiver.c tcp_sendbuf.c
	evacceptor.c evlearner.c evproposer.c)

add_library(evpaxos SHARED ${LOCAL_SOURCES})

target_link_libraries(evpaxos paxos ${LIBPAXOS_LINKER_LIBS} 
	${LIBEVENT_LIBRARIES})

INSTALL(TARGETS evpaxos DESTINATION lib)
INSTALL(FILES include/evpaxos.h DESTINATION include)
INSTALL(DIRECTORY include/ DESTINATION include/evpaxos)