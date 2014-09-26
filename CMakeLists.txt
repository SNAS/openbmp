# cmake file for openbmpd

# Define the version
set (OPENBMP_VER_MAJOR "0")
set (OPENBMP_VER_MINOR "8")
set (OPENBMP_VER_PATCH "0-pre5")
set (OPENBMPD_VERSION "${OPENBMP_VER_MAJOR}.${OPENBMP_VER_MINOR}.${OPENBMP_VER_PATCH}")

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

# Add the Server directory
add_subdirectory (Server)

FIND_PROGRAM(DPKG_CMD dpkg)
IF(DPKG_CMD)
  add_subdirectory (deb_package)
ENDIF(DPKG_CMD)


