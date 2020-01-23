INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/paxos/include)

SET(SRCS paxos.c standard_acceptor.c learner.c standard_proposer.c carray.c quorum.c
		stable_storage.c storage_utils.c stable_storage_but_not_really.c paxos_storage.c hash_mapped_memory.c write_ahead_promises_acceptor.c paxos_message_conversion.c include/write_ahead_acceptor.h)

IF (LMDB_FOUND)
	LIST(APPEND SRCS stable_storage_lmdb.c)
ENDIF ()

ADD_LIBRARY(paxos STATIC ${SRCS})
TARGET_LINK_LIBRARIES(paxos ${LIBPAXOS_LINKER_LIBS})

IF (LMDB_FOUND)
	INCLUDE_DIRECTORIES(${LMDB_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(paxos ${LMDB_LIBRARIES})
ENDIF ()
