# Find and set the env for the mysql c++ connector
set(MYSQLCONNECTORCPP_ROOT_DIR
        "${MYSQLCONNECTORCPP_ROOT_DIR}"
        CACHE
        PATH
        "Where to start looking for this component.")

find_path(MYSQLCONNECTORCPP_INCLUDE_DIR
        mysql_connection.h
        HINTS
        ${MYSQLCONNECTORCPP_ROOT_DIR}
        PATH_SUFFIXES
        include)

find_library(MYSQLCONNECTORCPP_LIBRARY
        NAMES
        mysqlcppconn
        mysqlcppconn-static
        HINTS
        ${MYSQLCONNECTORCPP_ROOT_DIR}
        PATH_SUFFIXES
        lib64
        lib)

if (NOT MYSQLCONNECTORCPP_INCLUDE_DIR OR NOT MYSQLCONNECTORCPP_LIBRARY)
	Message (FATAL_ERROR "Mysql connector is not found, cannot proceed.  Try 'apt-get install libmysqlcppconn-dev' or visit http://dev.mysql.com/downloads/connector/cpp/")
#else ()
#	Message ("mysql lib = " ${MYSQLCONNECTORCPP_LIBRARY})
endif()

# Update the include dir
include_directories(${MYSQLCONNECTORCPP_INCLUDE_DIR} src/ src/bgp)


# Define the source files to compile
set (SRC_FILES
	src/BMPListener.cpp
	src/BMPReader.cpp
	src/DbImpl_mysql.cpp
	src/openbmp.cpp
	src/parseBMP.cpp
	src/md5.cpp
	src/Logger.cpp
	src/client_thread.cpp
	src/bgp/parseBGP.cpp
	src/bgp/NotificationMsg.cpp
	src/bgp/OpenMsg.cpp
	src/bgp/UpdateMsg.cpp
	src/bgp/MPReachAttr.cpp
	src/bgp/MPUnReachAttr.cpp
	) 

# Disable warnings
add_definitions ("-Wno-unused-result")

# Add C++11
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(--std=c++11 SUPPORTS_STD_CXX11)
    check_cxx_compiler_flag(--std=c++0x SUPPORTS_STD_CXX01)
    if(SUPPORTS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c++11")
    elseif(SUPPORTS_STD_CXX01)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++0x")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c++0x")
    else()
        message(ERROR "Compiler does not support --std=c++11 or --std=c++0x. Upgrade gcc 4.7 or greater")
    endif()
endif()

#--not-used--  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=gnu++0x")
#--not used-- list( APPEND CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Set the libs to link
set (LIBS pthread ${MYSQLCONNECTORCPP_LIBRARY})

# Set the binary
add_executable (openbmpd ${SRC_FILES})

# Link the binary
target_link_libraries (openbmpd ${LIBS}) 

# Install the binary and configs
install(TARGETS openbmpd DESTINATION bin COMPONENT binaries)
#install(FILES openbmpd.conf DESTINATION etc/openbmp/ COMPONENT config)
