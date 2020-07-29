cmake_minimum_required(VERSION 3.10.2)
project(KinesisVideoProducerApp VERSION 0.0.1)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

include(ExternalProject)


ExternalProject_Add(project_libkvs
                    PREFIX          ${CMAKE_CURRENT_SOURCE_DIR}/build/KinesisVideoProducerC
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
                    TEST_COMMAND    "")

add_executable(KinesisVideoProducerApp src/main.c)
add_dependencies(KinesisVideoProducerApp project_libkvs)
#target_include_directories (kvsProducer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set(KVS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/KinesisVideoProducerC/src/project_libkvs)
set(KVS_PIC_PATH ${KVS_PATH}/dependency/libkvspic/kvspic-src/src)

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
                        ${KVS_PATH}/open-source/lib64/libcurl.a
                        ${KVS_PATH}/open-source/lib/libmbedtls.a
                        ${KVS_PATH}/open-source/lib/libmbedx509.a
                        ${KVS_PATH}/open-source/lib/libmbedcrypto.a
                        ZLIB::ZLIB
                        Threads::Threads
                        ${CMAKE_DL_LIBS}
                        )