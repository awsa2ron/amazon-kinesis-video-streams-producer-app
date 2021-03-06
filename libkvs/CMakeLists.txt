cmake_minimum_required(VERSION 3.10.2)

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

# Library
# Note: may not work on windows
install (TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib)

install (DIRECTORY ${KinesisVideoProducerC_SOURCE_DIR}/open-source/lib/
    DESTINATION lib
    FILES_MATCHING PATTERN "*.so*")

# Header files
install(DIRECTORY ${KinesisVideoProducerC_SOURCE_DIR}/src/include/
    ${KinesisVideoProducerC_SOURCE_DIR}/open-source/include/
    DESTINATION include)