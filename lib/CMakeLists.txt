include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})
include_directories(${BDB_INCLUDE_DIRS})


SET(LOCAL_SOURCES acceptor_state.c carray.c learner_state.c proposer_state.c 
	storage.c)

add_library(paxos SHARED ${LOCAL_SOURCES})

target_link_libraries(paxos ${LIBPAXOS_LINKER_LIBS} ${BDB_LIBRARIES} 
	${LIBEVENT_LIBRARIES})
	
INSTALL(TARGETS paxos DESTINATION lib)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/include/libpaxos.h DESTINATION include)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/libpaxos)
