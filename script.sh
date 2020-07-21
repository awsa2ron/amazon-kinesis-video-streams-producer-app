#!/bin/bash

# clone particular version or hash 
git clone --recursive https://github.com/awslabs/amazon-kinesis-video-streams-producer-c.git
cd amazon-kinesis-video-streams-producer-c
git co e7d4868d8c336cec5fa35250212fffa19135a6ba
# Change to static build
sed -e 274s/SHARED/STATIC/g CMakeLists.txt > CMakeLists.txt.static
mv CMakeLists.txt.static CMakeLists.txt

# OFF openSSL
# sed -e 9s/ON/OFF/g CMakeLists.txt > CMakeLists.txt.offOpenSSL
# mv CMakeLists.txt.offOpenSSL CMakeLists.txt

# ON mbedTLS
# sed -e 10s/OFF/ON/g CMakeLists.txt > CMakeLists.txt.onMbedTLS
# mv CMakeLists.txt.onMbedTLS CMakeLists.txt

mkdir -p build
cd build/ && rm -rf ./* && rm -rf ../open-source/* && rm -rf ../dependency
git pull --recurse-submodules 
cmake .. -DBUILD_STATIC=TRUE -DUSE_OPENSSL=FALSE -DUSE_MBEDTLS=TRUE
make

# Strip
mkdir -p release
cp build/kvsAacAudioVideoStreamingSample release/kvsProducerApp
ls -lh release/
strip release/kvsProducerApp 