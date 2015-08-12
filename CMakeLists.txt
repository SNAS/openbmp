# cmake file for openbmpd

# Define the version
set (OPENBMP_VER_MAJOR "0")
set (OPENBMP_VER_MINOR "11")
set (OPENBMP_VER_PATCH "0")
set (OPENBMP_VER_RELEASE "pre2")
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

FIND_PROGRAM(DPKG_CMD dpkg)
IF(DPKG_CMD)
  add_subdirectory (deb_package)
ENDIF(DPKG_CMD)

FIND_PROGRAM(RPKG_CMD rpmbuild)
IF(RPKG_CMD)
  add_subdirectory (rpm_package)
ENDIF(RPKG_CMD)


