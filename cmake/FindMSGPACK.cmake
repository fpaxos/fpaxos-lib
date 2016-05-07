# Find MessagePack
# MSGPACK_INCLUDES - msgpack.h
# MSGPACK_LIBRARIES - msgpack libraries
# MSGPACK_FOUND - True if msgpack was found.

set(MSGPACK_ROOT "" CACHE STRING "MessagePack root directory")

find_path(MSGPACK_INCLUDE_DIR msgpack.h HINTS "${MSGPACK_ROOT}/include")
find_library(MSGPACK_LIBRARY NAMES msgpack msgpackc HINTS "${MSGPACK_ROOT}/lib")

set(MSGPACK_LIBRARIES ${MSGPACK_LIBRARY})
set(MSGPACK_INCLUDE_DIRS ${MSGPACK_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MSGPACK DEFAULT_MSG
                                  MSGPACK_LIBRARY MSGPACK_INCLUDE_DIR)
mark_as_advanced(MSGPACK_INCLUDE_DIR MSGPACK_LIBRARY)
