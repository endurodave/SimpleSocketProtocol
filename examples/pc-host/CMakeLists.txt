# SSP PC host: talks SSP over a serial port to a board running
# examples/platformio/ssp-device. Builds on Windows (COM port) and Linux
# (termios tty).
#
#   cmake -B build -S examples/pc-host
#   cmake --build build --config Release
#   build/Release/ssp-pc-host COM6          (Windows)
#   build/ssp-pc-host /dev/ttyACM0          (Linux)

cmake_minimum_required(VERSION 3.10)
project(ssp_pc_host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SSP_TRACE "Print SSP's internal trace messages" OFF)

set(SSP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
file(GLOB SSP_SOURCES ${SSP_ROOT}/ssp/*.c ${SSP_ROOT}/ssp/*.cpp)

if(WIN32)
    set(PORT_SOURCES
        ${SSP_ROOT}/port/hal/windows/ssp_hal_windows.c
        ${SSP_ROOT}/port/osal/std_lib/ssp_osal_std.cpp)
    set(SSP_DEFINES SSP_HAL=SSP_HAL_WIN SSP_OSAL=SSP_OSAL_STD _CRT_SECURE_NO_WARNINGS)
else()
    set(PORT_SOURCES
        ${SSP_ROOT}/port/hal/linux/ssp_hal_serial_posix.c
        ${SSP_ROOT}/port/osal/linux/ssp_osal_unix.c)
    set(SSP_DEFINES SSP_HAL=SSP_HAL_SERIAL_POSIX SSP_OSAL=SSP_OSAL_UNIX)
endif()

add_executable(ssp-pc-host main.cpp ${SSP_SOURCES} ${PORT_SOURCES} ${SSP_ROOT}/port/ssp_fault.c)
target_include_directories(ssp-pc-host PRIVATE ${SSP_ROOT}/ssp ${SSP_ROOT}/port)
target_compile_definitions(ssp-pc-host PRIVATE ${SSP_DEFINES})
if(NOT SSP_TRACE)
    target_compile_definitions(ssp-pc-host PRIVATE SSP_NO_TRACE)
endif()
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(ssp-pc-host PRIVATE Threads::Threads)
endif()
