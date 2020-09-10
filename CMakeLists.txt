cmake_minimum_required(VERSION 3.10.2)

project(kvsproject)

#set(${CMAKE_INSTALL_PREFIX} ${CMAKE_BINARY_DIR})

add_subdirectory(dependences)
add_subdirectory(amazon-kinesis-video-streams-producer-c)
add_subdirectory(kvs)
add_subdirectory(libkvs)

add_dependencies(cproducer project_zlib)