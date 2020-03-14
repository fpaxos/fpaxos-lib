INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/paxos/include)

SET(SRCS paxos.c standard_acceptor.c learner.c standard_proposer.c carray.c quorum.c ballot.c paxos_value.c
        standard_stable_storage.c storage_utils.c stable_storage_but_not_really.c paxos_storage.c hash_mapped_memory.c writeahead_window_acceptor.c paxos_message_conversion.c include/epoch_stable_storage.h epoch_stable_storage.c include/writeahead_epoch_acceptor.h writeahead_epoch_acceptor.c include/epoch_paxos_storage.h epoch_paxos_storage.c epoch_proposer.c include/epoch_proposer.h include/epoch_learner.h include/epoch_quorum.h include/instance.h include/paxos_util.h paxos_util.c include/proposer_common.h proposer_common.c)

IF (LMDB_FOUND)
	LIST(APPEND SRCS stable_storage_lmdb.c)
ENDIF ()

ADD_LIBRARY(paxos STATIC ${SRCS})
TARGET_LINK_LIBRARIES(paxos ${LIBPAXOS_LINKER_LIBS})

IF (LMDB_FOUND)
	INCLUDE_DIRECTORIES(${LMDB_INCLUDE_DIRS})
	TARGET_LINK_LIBRARIES(paxos ${LMDB_LIBRARIES})
ENDIF ()
