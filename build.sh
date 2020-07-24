#!/bin/bash

export KVS_PATH=amazon-kinesis-video-streams-producer-c
export PIC_PATH=$KVS_PATH/dependency/libkvspic/kvspic-src
echo $PIC_PATH
export PIC_SRC_PATH=$PIC_PATH/src
echo $KVS_PATH

# linked libs order is important
# recommend CMake for complex project
gcc -pthread -lz -ldl app/KvsAacAudioVideoStreamingApp.c -o myApp \
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

mv myApp app/