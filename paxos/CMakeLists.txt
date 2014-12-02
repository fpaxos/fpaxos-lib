include_directories(${CMAKE_SOURCE_DIR}/paxos/include)

SET(LOCAL_SOURCES paxos.c acceptor.c learner.c proposer.c carray.c quorum.c)

if (USE_MEM_STORE)
	LIST(APPEND LOCAL_SOURCES storage_mem.c)
else()
if (USE_LMDB_STORE)
	include_directories(${LMDB_INCLUDE_DIRS})
	LIST(APPEND LOCAL_SOURCES storage_lmdb.c)
else()
	include_directories(${BDB_INCLUDE_DIRS})
	LIST(APPEND LOCAL_SOURCES storage_bdb.c)
endif()
endif()

add_library(paxos SHARED ${LOCAL_SOURCES})

target_link_libraries(paxos ${LIBPAXOS_LINKER_LIBS})

if (NOT USE_MEM_STORE)
if (NOT USE_LMDB_STORE)
	target_link_libraries(paxos ${BDB_LIBRARIES})
else()
	target_link_libraries(paxos ${LMDB_LIBRARIES})
endif()
endif()

set_target_properties(paxos PROPERTIES
	INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

INSTALL(TARGETS paxos DESTINATION lib)
INSTALL(FILES include/paxos.h DESTINATION include)
INSTALL(DIRECTORY include/ DESTINATION include/libpaxos)
