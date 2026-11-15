# Collect all .cpp files in this subdirectory
file(GLOB_RECURSE SUBDIR_SOURCES "*.c*")

# Collect all .h files in this subdirectory
file(GLOB_RECURSE SUBDIR_HEADERS "*.h")

# Create a library target 
add_library(PortLib STATIC ${SUBDIR_SOURCES} ${SUBDIR_HEADERS} ${HAL_SOURCES} ${OSAL_SOURCES})

# Include directories for the library
target_include_directories(PortLib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")