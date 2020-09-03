IDIR =../build/include
LDIR =../build/lib
CC=gcc
CFLAGS=-I$(IDIR)

KVS_PATH=$(KVS_APP_PATH)/amazon-kinesis-video-streams-producer-c

all:
	$(CC) -pthread -lz -ldl main.c -o main $(CFLAGS) \
	-L$(LDIR) -lcproducer -lkvsCommonCurl -lkvspic -lcurl -lmbedtls -lmbedx509 -lmbedcrypto

.PHONY: clean

clean:
	rm -f sample
