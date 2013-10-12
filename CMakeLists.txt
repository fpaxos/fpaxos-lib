cmake_minimum_required(VERSION 2.6)
project(libpaxos)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(BUILD_UNIT "Build unit tests." OFF)
option(USE_MEM_STORE "Use in-memory storage for acceptors." OFF)

if (NOT USE_MEM_STORE)
	find_package(BDB REQUIRED)
endif()
find_package(LIBEVENT REQUIRED)
find_package(MSGPACK REQUIRED)

if (NOT APPLE)
	set(LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} m rt)
endif()

add_subdirectory(paxos)
add_subdirectory(evpaxos)
add_subdirectory(sample)

if (BUILD_UNIT)
	find_package(GTEST REQUIRED)
	enable_testing()
	add_subdirectory(unit)
endif()
