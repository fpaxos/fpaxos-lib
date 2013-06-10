# Find Google Testing Framework
# GTEST_INCLUDES - where to find gtest.h
# GTEST_LIBRARIES - List of libraries when using GTEST.
# GTEST_FOUND - True if GTEST found.

set(GTEST_ROOT "" CACHE STRING "Google Test root directory")

set(GTEST_INCLUDE_DIR "${GTEST_ROOT}/include")
find_library(GTEST_LIBRARY NAMES gtest HINTS "${GTEST_ROOT}/lib")

set(GTEST_LIBRARIES ${GTEST_LIBRARY})
set(GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set GTEST_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(GTEST DEFAULT_MSG
                                  GTEST_LIBRARY GTEST_INCLUDE_DIR)

mark_as_advanced(GTEST_INCLUDE_DIR GTEST_LIBRARY)
