include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})
include_directories(${BDB_INCLUDE_DIRS})

set(TEST_SOURCES example_learner.c example_acceptor.c example_proposer.c 
	client.c)

foreach(p ${TEST_SOURCES})
	get_filename_component(target "${p}" NAME_WE)
	add_executable(${target} ${p})
	target_link_libraries(${target} paxos ${BDB_LIBRARIES} 
		${LIBEVENT_LIBRARIES} ${LIBPAXOS_LINKER_LIBS})
endforeach(p)

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/example_acceptor ${CMAKE_CURRENT_BINARY_DIR}/example_learner ${CMAKE_CURRENT_BINARY_DIR}/example_proposer DESTINATION bin)
