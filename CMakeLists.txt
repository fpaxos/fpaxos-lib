cmake_minimum_required(VERSION 2.6)
project(libpaxos)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(BDB REQUIRED)
find_package(LIBEVENT REQUIRED)

if (NOT APPLE)
	set(LIBPAXOS_LINKER_LIBS ${LIBPAXOS_LINKER_LIBS} m rt)
endif()

add_subdirectory(lib)
add_subdirectory(tests)
