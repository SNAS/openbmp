# cmake file for openbmpd

# Define the version
set (OPENBMP_VER_MAJOR "0")
set (OPENBMP_VER_MINOR "13")
set (OPENBMP_VER_PATCH "0")
set (OPENBMP_VER_RELEASE "pre10")
set (OPENBMPD_VERSION "${OPENBMP_VER_MAJOR}.${OPENBMP_VER_MINOR}.${OPENBMP_VER_PATCH}-${OPENBMP_VER_RELEASE}")

cmake_minimum_required (VERSION 2.6)
project (OPENBMP)

# find and include boost
find_package( Boost 1.41.0 REQUIRED)
   if(Boost_FOUND)
      include_directories(${Boost_INCLUDE_DIRS})
  endif()

# Configure the version for compiling
configure_file( openbmpd_version.h.in ${CMAKE_BINARY_DIR}/generated/openbmpd_version.h )
include_directories( ${CMAKE_BINARY_DIR}/generated/ ) 

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(MACOSX TRUE)
else ()
    Message (FATAL_ERROR "${CMAKE_SYSTEM_NAME} not supported; Must be Linux or Darwin")
endif()

# Add the Server directory
add_subdirectory (Server)

# Add google test
add_subdirectory(gtest)
enable_testing()
include_directories(${GTEST_INCLUDE_DIRS} Server)
file(GLOB TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/Server/tests/*.cpp)
add_executable(openbmp_tests ${TEST_SRC_FILES})
add_dependencies(openbmp_tests googletest)
# 
# if(NOT WIN32 OR MINGW)
#     target_link_libraries(${PROJECT_TEST_NAME}
#         ${GTEST_LIBS_DIR}/libgtest.a
#         ${GTEST_LIBS_DIR}/libgtest_main.a
#         )
# else()
#     target_link_libraries(${PROJECT_TEST_NAME}
#         debug ${GTEST_LIBS_DIR}/DebugLibs/${CMAKE_FIND_LIBRARY_PREFIXES}gtest${CMAKE_FIND_LIBRARY_SUFFIXES}
#         optimized ${GTEST_LIBS_DIR}/ReleaseLibs/${CMAKE_FIND_LIBRARY_PREFIXES}gtest${CMAKE_FIND_LIBRARY_SUFFIXES}
#         )
#     target_link_libraries(${PROJECT_TEST_NAME}
#         debug ${GTEST_LIBS_DIR}/DebugLibs/${CMAKE_FIND_LIBRARY_PREFIXES}gtest_main${CMAKE_FIND_LIBRARY_SUFFIXES}
#         optimized ${GTEST_LIBS_DIR}/ReleaseLibs/${CMAKE_FIND_LIBRARY_PREFIXES}gtest_main${CMAKE_FIND_LIBRARY_SUFFIXES}
#         )
# endif()
# 
# target_link_libraries(${PROJECT_TEST_NAME} ${CMAKE_THREAD_LIBS_INIT})
# 
# add_test(test1 ${PROJECT_TEST_NAME})
# 
FIND_PROGRAM(DPKG_CMD dpkg)
IF(DPKG_CMD)
  add_subdirectory (deb_package)
ENDIF(DPKG_CMD)

FIND_PROGRAM(RPKG_CMD rpmbuild)
IF(RPKG_CMD)
  add_subdirectory (rpm_package)
ENDIF(RPKG_CMD)
