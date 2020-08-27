cmake_minimum_required(VERSION 3.10.2)
project(KinesisVideoProducerApp VERSION 0.1.0)

find_package(Threads REQUIRED)

find_program(MAKE_EXE NAMES make)

# CMAKE_CURRENT_BINARY_DIR = CMAKE_CURRENT_SOURCE_DIR/build
set(ZLIB_PATH ${CMAKE_CURRENT_BINARY_DIR}/libz/src/project_zlib)
set(KVS_PATH ${CMAKE_CURRENT_BINARY_DIR}/producerC/src/project_libkvs)
set(KVS_PIC_PATH ${KVS_PATH}/dependency/libkvspic/kvspic-src/src)

include(ExternalProject)

set(CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/libz/src/project_zlib/configure --static --prefix=${CMAKE_CURRENT_BINARY_DIR}/libz/src/project_zlib)
ExternalProject_Add(project_zlib
                    PREFIX          ${CMAKE_CURRENT_BINARY_DIR}/libz
                    GIT_REPOSITORY  https://github.com/madler/zlib.git
                    GIT_TAG         v1.2.9
                    CONFIGURE_COMMAND ${CONFIGURE_COMMAND} 
                    BUILD_COMMAND     ${MAKE_EXE}
                    BUILD_IN_SOURCE   TRUE
                    INSTALL_COMMAND   ${MAKE_EXE} install
                    TEST_COMMAND    ""
)

ExternalProject_Add(project_libkvs
                    PREFIX          ${CMAKE_CURRENT_BINARY_DIR}/producerC
                    GIT_REPOSITORY  https://github.com/awslabs/amazon-kinesis-video-streams-producer-c.git
                    GIT_TAG         e7d4868d8c336cec5fa35250212fffa19135a6ba
                    UPDATE_COMMAND  git submodule update --init
                    PATCH_COMMAND   sed -e 274s/SHARED/STATIC/g CMakeLists.txt > CMakeLists.txt.static && mv CMakeLists.txt.static CMakeLists.txt
                    CMAKE_ARGS
                        -DBUILD_STATIC=TRUE
                        -DUSE_OPENSSL=FALSE
                        -DUSE_MBEDTLS=TRUE
                    BUILD_ALWAYS    TRUE
                    INSTALL_COMMAND ""
                    TEST_COMMAND    ""
)

add_executable(KinesisVideoProducerApp src/main.c)
add_dependencies(KinesisVideoProducerApp project_libkvs)
add_dependencies(project_libkvs project_zlib)
#target_include_directories (kvsProducer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if("${CMAKE_C_COMPILER_ID}" MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -ldl")
endif()

include_directories(
                    ${KVS_PATH}/src/include
                    ${KVS_PIC_PATH}/state/include
                    ${KVS_PIC_PATH}/heap/include
                    ${KVS_PIC_PATH}/mkvgen/include
                    ${KVS_PIC_PATH}/view/include
                    ${KVS_PIC_PATH}/client/include
                    ${KVS_PIC_PATH}/utils/include
                    ${KVS_PIC_PATH}/trace/include
                    ${KVS_PIC_PATH}/common/include
                    ${KVS_PATH}/open-source/include
)

target_link_libraries (KinesisVideoProducerApp
                    ${KVS_PATH}-build/libcproducer.a
                    ${KVS_PATH}-build/libkvsCommonCurl.a
                    ${KVS_PATH}-build/dependency/libkvspic/kvspic-src/libkvspic.a
                    ${KVS_PATH}/open-source/lib/libcurl.a
                    ${KVS_PATH}/open-source/lib/libmbedtls.a
                    ${KVS_PATH}/open-source/lib/libmbedx509.a
                    ${KVS_PATH}/open-source/lib/libmbedcrypto.a
                    ${ZLIB_PATH}/lib/libz.a
                    Threads::Threads
                    ${CMAKE_DL_LIBS}
)
