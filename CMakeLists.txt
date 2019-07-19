cmake_minimum_required(VERSION 2.6)
project(OPENBMP)

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(LINUX TRUE)
elseif (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    set(MACOSX TRUE)
else ()
    Message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} not supported; Must be Linux or Darwin")
endif ()

set(SRC_FILES
        src/Config/Config.cpp
        src/OpenBMP.cpp
        src/Worker/Worker.cpp
        src/MessageBus/MessageBus.cpp
        src/Logger/Logger.cpp
        src/Worker/Encapsulator.cpp
        )

add_executable(openbmpd ${SRC_FILES})
