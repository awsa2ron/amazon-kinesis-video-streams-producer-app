cmake_minimum_required(VERSION 3.10.2)

project(KinesisVideoProducerApp)

# dependences
add_subdirectory(libz)
add_subdirectory(amazon-kinesis-video-streams-producer-c)


# flags
if("${CMAKE_C_COMPILER_ID}" MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -rdynamic")
endif()
set(CMAKE_EXE_LINKER_FLAGS "-L${KinesisVideoProducerC_SOURCE_DIR}/open-source/lib")

# App: KinesisVideoProducerApp
add_executable(${PROJECT_NAME} src/main.c)
add_dependencies(cproducer project_zlib)

file(GLOB PIC_HEADERS "${KinesisVideoProducerC_SOURCE_DIR}/dependency/libkvspic/kvspic-src/src/*/include")
include_directories(
                    ${PIC_HEADERS}
                    ${KinesisVideoProducerC_SOURCE_DIR}/src/include
                    ${KinesisVideoProducerC_SOURCE_DIR}/open-source/include
)


target_link_libraries (${PROJECT_NAME} cproducer)