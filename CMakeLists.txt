cmake_minimum_required(VERSION 2.6)
project(OPENBMP)

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(MACOSX TRUE)
else ()
    Message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} not supported; Must be Linux or Darwin")
endif ()


# find openssl
find_package( OpenSSL 1)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    set (SSL_LIBS ${OPENSSL_LIBRARIES} sasl2)
else()
    set(SSL_LIBS )
endif()

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
    Message (FATAL_ERROR "Librdkafka was not found, cannot proceed.  Visit https://github.com/edenhill/librdkafka for details on how to install it.")
    #else ()
    #	Message ("lib = " ${LIBRDKAFKA_LIBRARY})
endif()

if (NOT LIBYAML_CPP_INCLUDE_DIR OR NOT LIBYAML_CPP_LIBRARY)
    Message (FATAL_ERROR "Libyaml-cpp was not found, cannot proceed.  Visit https://github.com/jbeder/yaml-cpp for install details.")
endif()

if (NOT LIBRT_LIBRARY AND NOT MACOSX)
    Message (FATAL_ERROR "librt was not found, cannot proceed.")
endif()


include_directories(${LIBRDKAFKA_INCLUDE_DIR} ${LIBYAML_CPP_INCLUDE_DIR})

set(LIBS pthread ${LIBYAML_CPP_LIBRARY} ${LIBRDKAFKA_CPP_LIBRARY} ${LIBRDKAFKA_LIBRARY} z ${SSL_LIBS} dl)

set(SRC_FILES
        src/Main.cpp
        src/OpenBMP.cpp
        src/OpenBMP.h
        src/Worker.cpp
        src/Worker.h
        src/MessageBus.cpp
        src/MessageBus.h
        src/Logger.cpp
        src/Logger.h
        src/Encapsulator.cpp
        src/Encapsulator.h
        src/Config.cpp
        src/Config.h
        )

add_executable(openbmpd ${SRC_FILES})

# Link the binary
target_link_libraries (openbmpd ${LIBS})

if (LIBRT_LIBRARY)
    target_link_libraries(openbmpd ${LIBRT_LIBRARY})
endif()

