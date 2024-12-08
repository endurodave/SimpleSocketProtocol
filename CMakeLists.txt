# Example CMake command line to create project build files:
#
# *** Windows ***
# cmake -G "Visual Studio 17 2022" -A Win32 -B Build -S .
#
# *** Linux ***
# cmake -G "Unix Makefiles" -B Build -S .
#
# See ssp_opt.h for HAL and OSAL build options.

# Specify the minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and language (C or C++)
project(SimpleSocketProtocol VERSION 1.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Collect all .cpp and *.h source files in the current directory
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/*.cpp" "${CMAKE_SOURCE_DIR}/*.h")

# Add subdirectories to include path
include_directories( 
    ${CMAKE_SOURCE_DIR}/example
    ${CMAKE_SOURCE_DIR}/port
    ${CMAKE_SOURCE_DIR}/ssp
)

# Add an executable target
add_executable(SimpleSocketProtocolApp ${SOURCES})

if (ENABLE_ALLOCATOR)
    add_compile_definitions(USE_ALLOCATOR)
endif()

# Add subdirectories to build
add_subdirectory(example)
add_subdirectory(port)
add_subdirectory(ssp)

target_link_libraries(SimpleSocketProtocolApp PRIVATE 
    ExampleLib
    PortLib
    SSPLib
)






 
