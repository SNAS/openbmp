# Define the version
set (OPENBMP_VER_MAJOR "2")
set (OPENBMP_VER_MINOR "0")
set (OPENBMP_VER_PATCH "0")
set (OPENBMP_VER_RELEASE "0")
set (OPENBMPD_VERSION "${OPENBMP_VER_MAJOR}.${OPENBMP_VER_MINOR}.${OPENBMP_VER_PATCH}-${OPENBMP_VER_RELEASE}")


cmake_minimum_required(VERSION 2.6)
project(OPENBMP)

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(MACOSX TRUE)
else ()
    Message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} not supported; Must be Linux or Darwin")
endif ()

# Add C++11
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(--std=c++11 SUPPORTS_STD_CXX11)
    if (SUPPORTS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c++11")
    else ()
        message(ERROR "Compiler does not support --std=c++11. Upgrade to gcc 4.8.1 or greater.")
    endif ()
endif ()

# Configure the version for compiling
configure_file(Version.h.in ${CMAKE_BINARY_DIR}/generated/Version.h )
include_directories( ${CMAKE_BINARY_DIR}/generated/ )


##############################################
############ Find dependencies ###############
##############################################

# Find openssl
find_package(OpenSSL 1)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    set(SSL_LIBS ${OPENSSL_LIBRARIES} sasl2)
else ()
    set(SSL_LIBS)
endif ()

set(HINT_ROOT_DIR
        "${HINT_ROOT_DIR}"
        CACHE
        PATH
        "Where to start looking for this component.")

find_path(LIBYAML_CPP_INCLUDE_DIR
        NAMES
        yaml-cpp/yaml.h
        HINTS
        ${HINT_ROOT_DIR}
        PATH_SUFFIXES
        include)

find_library(LIBYAML_CPP_LIBRARY
        NAMES
        libyaml-cpp.a yaml-cpp
        HINTS
        ${HINT_ROOT_DIR}
        PATH_SUFFIXES
        lib64
        lib)

find_path(LIBRDKAFKA_INCLUDE_DIR
        librdkafka/rdkafkacpp.h
        HINTS
        ${HINT_ROOT_DIR}
        PATH_SUFFIXES
        include)

find_library(LIBRDKAFKA_LIBRARY
        NAMES
        librdkafka.a rdkafka
        HINTS
        ${HINT_ROOT_DIR}
        PATH_SUFFIXES
        lib64
        lib)

find_library(LIBRDKAFKA_CPP_LIBRARY
        NAMES
        librdkafka++.a rdkafka++
        HINTS
        ${HINT_ROOT_DIR}
        PATH_SUFFIXES
        lib64
        lib)

find_library(LIBRT_LIBRARY
        NAMES
        rt
        HINTS
        ${HINT_ROOT_DIR}
        PATH_SUFFIXES
        lib64
        lib)

if (NOT LIBRDKAFKA_INCLUDE_DIR OR NOT LIBRDKAFKA_LIBRARY OR NOT LIBRDKAFKA_CPP_LIBRARY)
    Message(FATAL_ERROR "Librdkafka was not found, cannot proceed.  Visit https://github.com/edenhill/librdkafka for details on how to install it.")
endif ()

if (NOT LIBYAML_CPP_INCLUDE_DIR OR NOT LIBYAML_CPP_LIBRARY)
    Message(FATAL_ERROR "Libyaml-cpp was not found, cannot proceed.  Visit https://github.com/jbeder/yaml-cpp for install details.")
endif ()

if (NOT LIBRT_LIBRARY AND NOT MACOSX)
    Message(FATAL_ERROR "librt was not found, cannot proceed.")
endif ()

# Update the include dir
include_directories(${LIBRDKAFKA_INCLUDE_DIR} ${LIBYAML_CPP_INCLUDE_DIR} src/)

# Set the libs to link
set(LIBS ${LIBYAML_CPP_LIBRARY} ${LIBRDKAFKA_CPP_LIBRARY} ${LIBRDKAFKA_LIBRARY} z ${SSL_LIBS} dl pthread)


##############################################
############## MAIN PROGRAM ##################
##############################################

set(MAIN_FILES
        src/Main.cpp
        src/OpenBMP.cpp
        src/Worker.cpp
        src/MessageBus.cpp
        src/Logger.cpp
        src/Encapsulator.cpp
        src/Config.cpp
        src/DataStore.cpp
        src/TopicBuilder.cpp)

add_executable(openbmp_main ${MAIN_FILES})

# Link the binary
target_link_libraries(openbmp_main ${LIBS})

if (LIBRT_LIBRARY)
    target_link_libraries(openbmp_main ${LIBRT_LIBRARY})
endif ()


##############################################
################ TEST CASES ##################
##############################################

############# Message Bus Tests ##############
set(MSGBUS_FILES
        test/message_bus_test.cpp
        src/MessageBus.cpp
        )

# Message bus test executable
add_executable(msgbus_test ${MSGBUS_FILES})
# Link the binary
target_link_libraries(msgbus_test ${LIBS})

