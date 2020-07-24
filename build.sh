#!/bin/bash

# pre-build:
# clone particular version or hash 
git clone --recursive https://github.com/awslabs/amazon-kinesis-video-streams-producer-c.git

cd amazon-kinesis-video-streams-producer-c
git checkout e7d4868d8c336cec5fa35250212fffa19135a6ba

# Change to static build
sed -e 274s/SHARED/STATIC/g CMakeLists.txt > CMakeLists.txt.static
mv CMakeLists.txt.static CMakeLists.txt

# Compile
mkdir -p build
cd build/ && rm -rf ./* && rm -rf ../open-source/* && rm -rf ../dependency
git pull --recurse-submodules 
cmake .. -DBUILD_STATIC=TRUE -DUSE_OPENSSL=FALSE -DUSE_MBEDTLS=TRUE
make

# build:
export KVS_PATH=amazon-kinesis-video-streams-producer-c
export PIC_PATH=$KVS_PATH/dependency/libkvspic/kvspic-src
echo $PIC_PATH
export PIC_SRC_PATH=$PIC_PATH/src
echo $KVS_PATH
pwd
cd ../..

# linked libs order is important
# recommend CMake for complex project
gcc -O0 -g -pthread -lz -ldl app/KvsAacAudioVideoStreamingApp.c -o myApp \
-L$KVS_PATH/build -lcproducer \
-L$KVS_PATH/build -lkvsCommonCurl \
-L$KVS_PATH/build/dependency/libkvspic/kvspic-src -lkvspic \
-L$KVS_PATH/open-source/lib64 -lcurl \
-L$KVS_PATH/open-source/lib -lmbedtls \
-L$KVS_PATH/open-source/lib -lmbedx509 \
-L$KVS_PATH/open-source/lib -lmbedcrypto \
-I$KVS_PATH/src/include \
-I$PIC_SRC_PATH/state/include \
-I$PIC_SRC_PATH/heap/include \
-I$PIC_SRC_PATH/mkvgen/include \
-I$PIC_SRC_PATH/view/include \
-I$PIC_SRC_PATH/client/include \
-I$PIC_SRC_PATH/utils/include \
-I$PIC_SRC_PATH/trace/include \
-I$PIC_SRC_PATH/common/include \
-I$KVS_PATH/open-source/include


# post-build:
mv myApp app/
strip app/myApp
