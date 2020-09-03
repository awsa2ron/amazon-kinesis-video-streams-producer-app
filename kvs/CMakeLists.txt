cmake_minimum_required(VERSION 2.8)

project(kvs)

# flags
if("${CMAKE_C_COMPILER_ID}" MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -rdynamic")
endif()
set(CMAKE_EXE_LINKER_FLAGS "-L${KinesisVideoProducerC_SOURCE_DIR}/open-source/lib")

add_executable(${PROJECT_NAME} kvs.c)

file(GLOB PIC_HEADERS "${KinesisVideoProducerC_SOURCE_DIR}/dependency/libkvspic/kvspic-src/src/*/include")
include_directories(
                    ${PIC_HEADERS}
                    ${KinesisVideoProducerC_SOURCE_DIR}/src/include
                    ${KinesisVideoProducerC_SOURCE_DIR}/open-source/include
)


target_link_libraries (${PROJECT_NAME} cproducer)