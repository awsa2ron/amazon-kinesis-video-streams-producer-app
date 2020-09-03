cmake_minimum_required(VERSION 3.10.2)

project(kvsproject)

add_subdirectory(libz)
add_subdirectory(amazon-kinesis-video-streams-producer-c)
add_subdirectory(kvs)
add_subdirectory(libkvs)

add_dependencies(cproducer project_zlib)