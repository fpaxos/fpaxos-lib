INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/paxos/include)

SET(SRCS paxos.c acceptor.c learner.c proposer.c carray.c quorum.c
	storage.c storage_utils.c storage_mem.c)

IF (LMDB_FOUND)
	LIST(APPEND SRCS storage_lmdb.c)
ENDIF ()

ADD_LIBRARY(paxos STATIC ${SRCS})
TARGET_LINK_LIBRARIES(paxos ${LIBPAXOS_LINKER_LIBS})

IF (LMDB_FOUND)
	INCLUDE_DIRECTORIES(${LMDB_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(paxos ${LMDB_LIBRARIES})
ENDIF ()
