# find openssl
find_package( OpenSSL 1)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    set (SSL_LIBS ${OPENSSL_LIBRARIES} sasl2)
else()
    set(SSL_LIBS )
endif()



# Find and set the env for the mysql c++ connector
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

# Update the include dir
include_directories(${LIBRDKAFKA_INCLUDE_DIR} ${LIBYAML_CPP_INCLUDE_DIR} src/ src/bmp src/bgp src/bgp/linkstate src/kafka)
#link_directories(${LIBRDKAFKA_LIBRARY})


# Define the source files to compile
set (SRC_FILES
	src/bmp/BMPListener.cpp
	src/bmp/BMPReader.cpp
	src/kafka/MsgBusImpl_kafka.cpp
	src/kafka/KafkaEventCallback.cpp
	src/kafka/KafkaDeliveryReportCallback.cpp
    src/kafka/KafkaTopicSelector.cpp
    src/kafka/KafkaPeerPartitionerCallback.cpp
	src/openbmp.cpp
	src/bmp/parseBMP.cpp
	src/md5.cpp
	src/Logger.cpp
    src/Config.cpp
	src/client_thread.cpp
	src/bgp/parseBGP.cpp
	src/bgp/NotificationMsg.cpp
	src/bgp/OpenMsg.cpp
	src/bgp/UpdateMsg.cpp
	src/bgp/MPReachAttr.cpp
	src/bgp/MPUnReachAttr.cpp
    src/bgp/ExtCommunity.cpp
    src/bgp/AddPathDataContainer.cpp
    src/bgp/linkstate/MPLinkState.cpp
    src/bgp/linkstate/MPLinkStateAttr.cpp
        src/parseBgpLib.cpp)

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

# Set the libs to link
set (LIBS pthread ${LIBYAML_CPP_LIBRARY} ${LIBRDKAFKA_CPP_LIBRARY} ${LIBRDKAFKA_LIBRARY} z ${SSL_LIBS})

# Set the binary
add_executable (openbmpd ${SRC_FILES})

# Link the binary
target_link_libraries (openbmpd ${LIBS})

if (LIBRT_LIBRARY)
    target_link_libraries(openbmpd ${LIBRT_LIBRARY})
endif()

# Install the binary and configs
install(TARGETS openbmpd DESTINATION bin COMPONENT binaries)
install(FILES openbmpd.conf DESTINATION etc/openbmp/ COMPONENT config)
