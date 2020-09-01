cmake_minimum_required(VERSION 3.10.2)

project(KinesisVideoProducerApp)

add_subdirectory(amazon-kinesis-video-streams-producer-c)

# dependences
include(ExternalProject)
find_program(MAKE_EXE NAMES make)
set(CONFIGURE_COMMAND ${CMAKE_BINARY_DIR}/libz/src/project_zlib/configure --prefix=${KinesisVideoProducerC_SOURCE_DIR}/open-source)
ExternalProject_Add(project_zlib
                    PREFIX          ${CMAKE_BINARY_DIR}/libz
                    GIT_REPOSITORY  https://github.com/madler/zlib.git
                    GIT_TAG         v1.2.9
                    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} 
                    BUILD_COMMAND     ${MAKE_EXE}
                    BUILD_IN_SOURCE   TRUE
                    INSTALL_COMMAND   ${MAKE_EXE} install
                    TEST_COMMAND    ""
)
find_package(ZLIB)

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