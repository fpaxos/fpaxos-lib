include_directories(${CMAKE_SOURCE_DIR}/evpaxos/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

SET(LOCAL_SOURCES config_reader.c tcp_receiver.c tcp_sendbuf.c peers.c
	evacceptor.c evlearner.c evproposer.c)

add_library(evpaxos SHARED ${LOCAL_SOURCES})

target_link_libraries(evpaxos paxos ${LIBPAXOS_LINKER_LIBS} 
	${LIBEVENT_LIBRARIES})

set_target_properties(evpaxos PROPERTIES
	INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

INSTALL(TARGETS evpaxos DESTINATION lib)
INSTALL(FILES include/evpaxos.h DESTINATION include)
INSTALL(DIRECTORY include/ DESTINATION include/evpaxos)
