include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${BDB_INCLUDE_DIRS})

SET(LOCAL_SOURCES acceptor.c learner.c proposer.c storage.c carray.c quorum.c)

add_library(paxos SHARED ${LOCAL_SOURCES})

target_link_libraries(paxos ${LIBPAXOS_LINKER_LIBS} ${BDB_LIBRARIES})

set_target_properties(paxos PROPERTIES
	INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

INSTALL(TARGETS paxos DESTINATION lib)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/include/libpaxos.h DESTINATION include)
INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/libpaxos)
