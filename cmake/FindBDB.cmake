# - Find BDB
# Find the native BDB includes and library
#
# BDB_INCLUDES - where to find db.h
# BDB_LIBRARIES - List of libraries when using BDB.
# BDB_FOUND - True if BDB found.

set(BDB_ROOT "" CACHE STRING "BerkeleyDB root directory")

find_path(BDB_INCLUDE_DIR db.h HINTS "${BDB_ROOT}/include")
find_library(BDB_LIBRARY db  HINTS "${BDB_ROOT}/lib")

set(BDB_LIBRARIES ${BDB_LIBRARY})
set(BDB_INCLUDE_DIRS ${BDB_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set BDB_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(BDB DEFAULT_MSG
                                  BDB_LIBRARY BDB_INCLUDE_DIR)

mark_as_advanced(BDB_INCLUDE_DIR BDB_LIBRARY)
