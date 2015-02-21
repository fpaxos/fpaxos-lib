# - Find LMDB
# Find the native lmdb includes and library
#
# LMDB_INCLUDES - where to find lmdb.h
# LMDB_LIBRARIES - List of libraries when using lmdb.
# LMDB_FOUND - True if lmdb found.

set(LMDB_ROOT "" CACHE STRING "lmdb root directory")

find_path(LMDB_INCLUDE_DIR lmdb.h HINTS "${LMDB_ROOT}/include")
find_library(LMDB_LIBRARY lmdb HINTS "${LMDB_ROOT}/lib")

set(LMDB_LIBRARIES ${LMDB_LIBRARY})
set(LMDB_INCLUDE_DIRS ${LMDB_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set LMDB_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LMDB DEFAULT_MSG
                                  LMDB_LIBRARY LMDB_INCLUDE_DIR)

mark_as_advanced(LMDB_INCLUDE_DIR LMDB_LIBRARY)
