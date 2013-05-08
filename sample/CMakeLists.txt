include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/evpaxos/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})

set(TEST_SOURCES learner.c acceptor.c proposer.c client.c)

foreach(p ${TEST_SOURCES})
	get_filename_component(target "${p}" NAME_WE)
	add_executable(${target} ${p})
	target_link_libraries(${target} evpaxos ${BDB_LIBRARIES} 
		${LIBEVENT_LIBRARIES} ${LIBPAXOS_LINKER_LIBS})
endforeach(p)

INSTALL(PROGRAMS
	${CMAKE_CURRENT_BINARY_DIR}/acceptor
	${CMAKE_CURRENT_BINARY_DIR}/proposer 
	DESTINATION bin)
