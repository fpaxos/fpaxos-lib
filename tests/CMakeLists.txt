include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${LIBEVENT_INCLUDE_DIRS})
include_directories(${BDB_INCLUDE_DIRS})

# FILE(GLOB TEST_SOURCES *.c)
set(TEST_SOURCES example_learner.c 	example_acceptor.c example_proposer.c tp_monitor.c client.c)

foreach(p ${TEST_SOURCES})
        get_filename_component(target "${p}" NAME_WE)
        add_executable(${target} ${p})
        target_link_libraries(${target} paxos ${LIBPAXOS_LINKER_LIBS} ${BDB_LIBRARIES} ${LIBEVENT_LIBRARIES})
endforeach(p)

