# cmake file for openbmpd

# Define the version
set (OPENBMPD_VERSION "0.8.0-pre")

cmake_minimum_required (VERSION 2.8)
project (OPENBMP)

# find and include boost
find_package( Boost 1.49.0 REQUIRED)
   if(Boost_FOUND)
      include_directories(${Boost_INCLUDE_DIRS})
  endif()

# Configure the version for compiling
configure_file( openbmpd_version.h.in ${CMAKE_BINARY_DIR}/generated/openbmpd_version.h )
include_directories( ${CMAKE_BINARY_DIR}/generated/ ) 

# Add the Server directory
add_subdirectory (Server)

