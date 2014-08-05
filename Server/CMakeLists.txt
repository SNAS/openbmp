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
	Message (FATAL_ERROR "Mysql connector is not found, cannot proceed.  Try apt-get install libmysqlcppconn-dev")
#else ()
#	Message ("mysql lib = " ${MYSQLCONNECTORCPP_LIBRARY})
endif()

# Update the include dir to include Mysql
include_directories(${MYSQLCONNECTORCPP_INCLUDE_DIR} )


# Define the source files to compile
set (SRC_FILES
	src/BMPServer.cpp
	src/DbImpl_mysql.cpp
	src/openbmp.cpp
	src/parseBGP.cpp
	src/parseBMP.cpp
	src/md5.cpp
	src/Logger.cpp
        src/BitByteUtils.cpp
	) 

# Disable warnings
add_definitions ("-Wno-unused-result")

# Set the libs to link
set (LIBS pthread ${MYSQLCONNECTORCPP_LIBRARY})
include_directories(${MYSQLCONNECTORCPP_INCLUDE_DIR} )

# Set the binary
add_executable (openbmpd ${SRC_FILES})

# Link the binary
target_link_libraries (openbmpd ${LIBS}) 

# Install the binary and configs
install(TARGETS openbmpd DESTINATION /usr/bin COMPONENT binaries)
#install(FILES openbmpd.conf DESTINATION /etc/openbmp/ COMPONENT config)
