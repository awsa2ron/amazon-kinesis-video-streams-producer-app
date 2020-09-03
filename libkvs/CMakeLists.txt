cmake_minimum_required(VERSION 2.8)

project(libkvs)

add_library(${PROJECT_NAME} INTERFACE)
add_library(kvs::header ALIAS ${PROJECT_NAME})

file(GLOB PIC_HEADERS "${KinesisVideoProducerC_SOURCE_DIR}/dependency/libkvspic/kvspic-src/src/*/include")

target_include_directories(${PROJECT_NAME}
    INTERFACE
        ${PIC_HEADERS}
        ${KinesisVideoProducerC_SOURCE_DIR}/src/include
        ${KinesisVideoProducerC_SOURCE_DIR}/open-source/include
)