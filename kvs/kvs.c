/*
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <unistd.h>
#include <getopt.h>
#include <com/amazonaws/kinesis/video/cproducer/Include.h>

#define DEFAULT_RETENTION_PERIOD            2 * HUNDREDS_OF_NANOS_IN_AN_HOUR
#define DEFAULT_BUFFER_DURATION             120 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_KEY_FRAME_INTERVAL          45
#define DEFAULT_FPS_VALUE                   25
#define DEFAULT_STREAM_DURATION             20 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define DEFAULT_STORAGE_SIZE                2 * 1024 * 1024
#define DEFAULT_MEDIA_DIRECTORY             "../" 
#define DEFAULT_CHANNEL_NAME                "your-kvs-name" 
#define SAMPLE_AUDIO_FRAME_DURATION         (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define SAMPLE_VIDEO_FRAME_DURATION         (HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE)
#define AUDIO_TRACK_SAMPLING_RATE           48000
#define AUDIO_TRACK_CHANNEL_CONFIG          2
#define MAX_KVS_HEAP_SIZE                   256 * 1024

#define NUMBER_OF_H264_FRAME_FILES          90
#define NUMBER_OF_AAC_FRAME_FILES           299

#define DEFAULT_LOG_LEVEL                   LOG_LEVEL_INFO
#define FILE_LOGGING_BUFFER_SIZE            (100 * 1024)
#define MAX_NUMBER_OF_LOG_FILES             5

typedef struct {
    PBYTE buffer;
    UINT32 size;
} FrameData, *PFrameData;

typedef struct {
    volatile ATOMIC_BOOL firstVideoFramePut;
    UINT64 streamStopTime;
    UINT64 streamStartTime;
    STREAM_HANDLE streamHandle;
    CLIENT_HANDLE clientHandle;
    CHAR sampleDir[MAX_PATH_LEN + 1];
    FrameData audioFrames;
    FrameData videoFrames;
} SampleCustomData, *PSampleCustomData;

static struct option long_options[] = {
    /*   NAME           ARGUMENT            FLAG    SHORTNAME */
    {"channel-name",    required_argument,  NULL,   'n'},
    {"directory",       required_argument,  NULL,   'd'},
    {"duration",        required_argument,  NULL,   'D'},
    {"size",            required_argument,  NULL,   's'},
    {"help",            no_argument,        NULL,   'h'},
    {NULL,              0,                  NULL,   0}
};

void displayUsage( int err )
{
    printf ("Ingest video to the Amazon Kinesis Video Streams service.\n");
    printf ("Usage: \n");
    printf ("AWS_ACCESS_KEY_ID=SAMPLEKEY AWS_SECRET_ACCESS_KEY=SAMPLESECRET\n");
    printf ("kvs [options...]\n");
    printf ("\n");
    printf ("-n, --channel-name     stream channel name\n");
    printf ("                       default to 'your-kvs-name'\n");
    printf ("-d, --directory        streaming media directory\n");
    printf ("                       default to '../'\n");
    printf ("-D, --duration         streaming duration in second\n");
    printf ("                       default to 600\n");
    printf ("-s, --size             stream buffer size in KB\n");
    printf ("                       default to 2048, minimal to 1024\n");
    printf ("\n");
    printf ("Exit status:\n \
    0  if OK,\n \
    1  if minor problems (e.g., missing argument),\n \
    others  if serious trouble, please check 'Include.h' files.\n");
    exit (err);
}

PVOID putVideoFrameRoutine(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleCustomData data = (PSampleCustomData) args;
    Frame frame;
    UINT32 videoFileIndex= 0;
    STATUS status;
    UINT64 runningTime, fileSize;
    CHAR filePath[MAX_PATH_LEN + 1];

    CHK(data != NULL, STATUS_NULL_ARG);

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = 0;
    frame.decodingTs = 0;
    frame.presentationTs = 0;
    frame.index = 0;

    ClientMetrics kinesisVideoClientMetrics;
    kinesisVideoClientMetrics.version = CLIENT_METRICS_CURRENT_VERSION;

    while (defaultGetTime() < data->streamStopTime) {
        SNPRINTF(filePath, MAX_PATH_LEN, "%s/h264SampleFrames/frame-%03d.h264", data->sampleDir, videoFileIndex + 1);
        CHK_STATUS(readFile(filePath, TRUE, NULL, &fileSize));
        data->videoFrames.buffer = (PBYTE) MEMALLOC(fileSize);
        data->videoFrames.size = fileSize;
        CHK_STATUS(readFile(filePath, TRUE, data->videoFrames.buffer, &fileSize));

        frame.frameData = data->videoFrames.buffer;
        frame.size = data->videoFrames.size;

        // video track is used to mark new fragment. A new fragment is generated for every frame with FRAME_FLAG_KEY_FRAME
        frame.flags = videoFileIndex% DEFAULT_KEY_FRAME_INTERVAL == 0 ? FRAME_FLAG_KEY_FRAME : FRAME_FLAG_NONE;

        CHK_STATUS(getKinesisVideoMetrics(data->clientHandle, &kinesisVideoClientMetrics));

        printf("Overall storage size:%d KB, Available:%d KB and this Video frame size:%d KB.\n", \
                kinesisVideoClientMetrics.contentStoreSize >> 10, \
                (kinesisVideoClientMetrics.contentStoreAvailableSize - MAX_KVS_HEAP_SIZE) >> 10, \
                frame.size >> 10 \
        );

        if(frame.size > kinesisVideoClientMetrics.contentStoreAvailableSize - MAX_KVS_HEAP_SIZE )
        {
            printf("No enough buffer for video data.\n");
            THREAD_SLEEP(frame.duration);
            continue;
        }

        status = putKinesisVideoFrame(data->streamHandle, &frame);
        ATOMIC_STORE_BOOL(&data->firstVideoFramePut, TRUE);
        if (STATUS_FAILED(status)) {
            printf("putKinesisVideoFrame failed with 0x%08x\n", status);
            status = STATUS_SUCCESS;
        }

        frame.presentationTs += SAMPLE_VIDEO_FRAME_DURATION;
        frame.decodingTs = frame.presentationTs;
        frame.index++;

        videoFileIndex++;
        if(videoFileIndex == NUMBER_OF_H264_FRAME_FILES)
            videoFileIndex = 0;

        SAFE_MEMFREE(data->videoFrames.buffer);
        // synchronize putKinesisVideoFrame to running time
        runningTime = defaultGetTime() - data->streamStartTime;
        if (runningTime < frame.presentationTs) {
            // reduce sleep time a little for smoother video
            THREAD_SLEEP((frame.presentationTs - runningTime) * 0.9);
        }
    }

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("putVideoFrameRoutine failed with 0x%08x", retStatus);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}

PVOID putAudioFrameRoutine(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleCustomData data = (PSampleCustomData) args;
    Frame frame;
    UINT32 audioFileIndex = 0;
    STATUS status;
    UINT64 runningTime, fileSize;
    CHAR filePath[MAX_PATH_LEN + 1];

    CHK(data != NULL, STATUS_NULL_ARG);

    frame.version = FRAME_CURRENT_VERSION;
    frame.trackId = DEFAULT_AUDIO_TRACK_ID;
    frame.duration = 0;
    frame.decodingTs = 0; // relative time mode
    frame.presentationTs = 0; // relative time mode
    frame.index = 0;
    frame.flags = FRAME_FLAG_NONE; // audio track is not used to cut fragment

    while (defaultGetTime() < data->streamStopTime) {
        // no audio can be put until first video frame is put
        if (ATOMIC_LOAD_BOOL(&data->firstVideoFramePut)) {
            SNPRINTF(filePath, MAX_PATH_LEN, "%s/aacSampleFrames/sample-%03d.aac", data->sampleDir, audioFileIndex + 1);
            CHK_STATUS(readFile(filePath, TRUE, NULL, &fileSize));
            data->audioFrames.buffer = (PBYTE) MEMALLOC(fileSize);
            data->audioFrames.size = fileSize;
            CHK_STATUS(readFile(filePath, TRUE, data->audioFrames.buffer, &fileSize));

            frame.frameData = data->audioFrames.buffer;
            frame.size = data->audioFrames.size;

            status = putKinesisVideoFrame(data->streamHandle, &frame);
            if (STATUS_FAILED(status)) {
                printf("putKinesisVideoFrame for audio failed with 0x%08x\n", status);
                status = STATUS_SUCCESS;
            }

            frame.presentationTs += SAMPLE_AUDIO_FRAME_DURATION;
            frame.decodingTs = frame.presentationTs;
            frame.index++;

            audioFileIndex++;
            if(audioFileIndex == NUMBER_OF_AAC_FRAME_FILES)
                audioFileIndex = 0;

            SAFE_MEMFREE(data->audioFrames.buffer);

            // synchronize putKinesisVideoFrame to running time
            runningTime = defaultGetTime() - data->streamStartTime;
            if (runningTime < frame.presentationTs) {
                THREAD_SLEEP(frame.presentationTs - runningTime);
            }
        }
    }

CleanUp:

    if (retStatus != STATUS_SUCCESS) {
        printf("putAudioFrameRoutine failed with 0x%08x", retStatus);
    }

    return (PVOID) (ULONG_PTR) retStatus;
}

INT32 main(INT32 argc, CHAR *argv[])
{
    PDeviceInfo pDeviceInfo = NULL;
    PStreamInfo pStreamInfo = NULL;
    PClientCallbacks pClientCallbacks = NULL;
    PStreamCallbacks pStreamCallbacks = NULL;
    CLIENT_HANDLE clientHandle = INVALID_CLIENT_HANDLE_VALUE;
    STREAM_HANDLE streamHandle = INVALID_STREAM_HANDLE_VALUE;
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR accessKey = NULL, secretKey = NULL, sessionToken = NULL, region = NULL, cacertPath = NULL;
    PCHAR streamName = DEFAULT_CHANNEL_NAME, mediaDirectory = DEFAULT_MEDIA_DIRECTORY;
    UINT64 streamStopTime, fileSize = 0, choice, option_index = 0;
    UINT64 streamingDuration = DEFAULT_STREAM_DURATION, bufferSize = DEFAULT_STORAGE_SIZE;
    TID audioSendTid, videoSendTid;
    PTrackInfo pAudioTrack = NULL;
    BYTE audioCpd[KVS_AAC_CPD_SIZE_BYTE];

    SampleCustomData data;

    while ((choice = getopt_long(argc, argv, ":n:d:D:s:h",
                 long_options, &option_index)) != -1) {
        switch (choice) {
        case 0:
            printf ("option %s", long_options[option_index].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;
        case 'n':
            streamName = optarg;
            printf ("KVS channel name is '%s'\n", streamName);
            break;
        case 'd':
            mediaDirectory = optarg;
            printf ("KVS stream media from '%s'\n", mediaDirectory);
            break;
        case 'D':
            CHK_STATUS(STRTOUI64(optarg, NULL, 10, &streamingDuration));
            printf ("KVS streaming for %d seconds\n", streamingDuration);
            break;
        case 's':
            CHK_STATUS(STRTOUI64(optarg, NULL, 10, &bufferSize));
            bufferSize *= 1024;
            printf ("KVS video buffer size is %d Bytes\n", bufferSize);
            break;
        case 'h':
            displayUsage(0);
            break;
        case ':':
            /* missing option argument */
            fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0], optopt);
            displayUsage(1);
            break;
        case '?':
            /* getopt_long already printed an error message. */
            displayUsage(1);
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", choice);
            displayUsage(1);
        }
    }

    if ((accessKey = getenv(ACCESS_KEY_ENV_VAR)) == NULL || (secretKey = getenv(SECRET_KEY_ENV_VAR)) == NULL) {
        printf("Error missing credentials\n");
        CHK(FALSE, STATUS_INVALID_ARG);
    }
    cacertPath = getenv(CACERT_PATH_ENV_VAR);
    sessionToken = getenv(SESSION_TOKEN_ENV_VAR);
    if ((region = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
        region = (PCHAR) DEFAULT_AWS_REGION;
    }

    MEMSET(&data, 0x00, SIZEOF(SampleCustomData));
    STRNCPY(data.sampleDir, mediaDirectory, MAX_PATH_LEN);
    if (data.sampleDir[STRLEN(data.sampleDir) - 1] == '/') {
        data.sampleDir[STRLEN(data.sampleDir) - 1] = '\0';
    }

    // Get the duration and convert to an integer
    streamStopTime = defaultGetTime() + streamingDuration*HUNDREDS_OF_NANOS_IN_A_SECOND;

    // default storage size is 128MB. Use setDeviceInfoStorageSize after create to change storage size.
    CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));
    // storage size must larger than MIN_STORAGE_ALLOCATION_SIZE
    pDeviceInfo->storageInfo.storageSize = bufferSize > MIN_STORAGE_ALLOCATION_SIZE ?
                                           bufferSize : 
                                           MIN_STORAGE_ALLOCATION_SIZE;
    // change storage size.
    CHK_STATUS(setDeviceInfoStorageSize(pDeviceInfo, pDeviceInfo->storageInfo.storageSize));
    // adjust members of pDeviceInfo here if needed
    pDeviceInfo->clientInfo.loggerLogLevel = DEFAULT_LOG_LEVEL;

    CHK_STATUS(createRealtimeAudioVideoStreamInfoProvider(streamName, DEFAULT_RETENTION_PERIOD, DEFAULT_BUFFER_DURATION, &pStreamInfo));

    // adjust members of pStreamInfo here if needed
    // set up audio cpd.
    pAudioTrack = pStreamInfo->streamCaps.trackInfoList[0].trackId == DEFAULT_AUDIO_TRACK_ID ?
                  &pStreamInfo->streamCaps.trackInfoList[0] :
                  &pStreamInfo->streamCaps.trackInfoList[1];
    // generate audio cpd
    pAudioTrack->codecPrivateData = audioCpd;
    pAudioTrack->codecPrivateDataSize = KVS_AAC_CPD_SIZE_BYTE;
    CHK_STATUS(mkvgenGenerateAacCpd(AAC_LC, AUDIO_TRACK_SAMPLING_RATE, AUDIO_TRACK_CHANNEL_CONFIG, pAudioTrack->codecPrivateData, pAudioTrack->codecPrivateDataSize));

    // use relative time mode. Buffer timestamps start from 0
    pStreamInfo->streamCaps.absoluteFragmentTimes = FALSE;

    CHK_STATUS(createDefaultCallbacksProviderWithAwsCredentials(accessKey,
                                                                secretKey,
                                                                sessionToken,
                                                                MAX_UINT64,
                                                                region,
                                                                cacertPath,
                                                                NULL,
                                                                NULL,
                                                                &pClientCallbacks));

    if(NULL != getenv(ENABLE_FILE_LOGGING)) {
        if((retStatus = addFileLoggerPlatformCallbacksProvider(pClientCallbacks,
                                                               FILE_LOGGING_BUFFER_SIZE,
                                                               MAX_NUMBER_OF_LOG_FILES,
                                                               (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH,
                                                               TRUE) != STATUS_SUCCESS)) {
            printf("File logging enable option failed with 0x%08x error code\n", retStatus);
        }
    }

    CHK_STATUS(createStreamCallbacks(&pStreamCallbacks));
    CHK_STATUS(addStreamCallbacks(pClientCallbacks, pStreamCallbacks));

    CHK_STATUS(createKinesisVideoClient(pDeviceInfo, pClientCallbacks, &clientHandle));
    CHK_STATUS(createKinesisVideoStreamSync(clientHandle, pStreamInfo, &streamHandle));

    data.streamStopTime = streamStopTime;
    data.streamHandle = streamHandle;
    data.clientHandle = clientHandle;
    data.streamStartTime = defaultGetTime();
    ATOMIC_STORE_BOOL(&data.firstVideoFramePut, FALSE);

    THREAD_CREATE(&videoSendTid, putVideoFrameRoutine,
                          (PVOID) &data);
    THREAD_CREATE(&audioSendTid, putAudioFrameRoutine,
                          (PVOID) &data);

    THREAD_JOIN(videoSendTid, NULL);
    THREAD_JOIN(audioSendTid, NULL);

    CHK_STATUS(stopKinesisVideoStreamSync(streamHandle));
    CHK_STATUS(freeKinesisVideoStream(&streamHandle));
    CHK_STATUS(freeKinesisVideoClient(&clientHandle));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        printf("Failed with status 0x%08x\n", retStatus);
    }


    freeDeviceInfo(&pDeviceInfo);
    freeStreamInfoProvider(&pStreamInfo);
    freeKinesisVideoStream(&streamHandle);
    freeKinesisVideoClient(&clientHandle);
    freeCallbacksProvider(&pClientCallbacks);

    return (INT32) retStatus;
}

