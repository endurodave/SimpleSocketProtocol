cmake_minimum_required(VERSION 3.6)
project(SimpleSocketProtocol)
set(CMAKE_VERBOSE_MAKEFILE ON)


set(SRC_DIR "${PROJECT_SOURCE_DIR}/ssp")

# Aux dirs
aux_source_directory("${SRC_DIR}" ${PROJECT_NAME}_SOURCES)

set(HEADERS

)
set(SOURCE_FILES

)

file(GLOB HDR_PRODUCT "${SRC_DIR}/*.h")
list(APPEND HEADERS ${HDR_PRODUCT})     


add_library(SimpleSocketProtocol STATIC ${SOURCE_FILES} ${${PROJECT_NAME}_SOURCES} ${HEADERS})
target_include_directories(SimpleSocketProtocol PUBLIC ${PROJECT_SOURCE_DIR} ${SRC_DIR})
 
