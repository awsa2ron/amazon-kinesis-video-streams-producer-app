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
#define DEFAULT_CALLBACK_CHAIN_COUNT        5
#define DEFAULT_KEY_FRAME_INTERVAL          45
#define DEFAULT_FPS_VALUE                   25
#define DEFAULT_STREAM_DURATION             20 * HUNDREDS_OF_NANOS_IN_A_SECOND
#define SAMPLE_AUDIO_FRAME_DURATION         (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define SAMPLE_VIDEO_FRAME_DURATION         (HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE)
#define AUDIO_TRACK_SAMPLING_RATE           48000
#define AUDIO_TRACK_CHANNEL_CONFIG          2
#define DEFAULT_STORAGE_SIZE                20 * 1024 * 1024

#define NUMBER_OF_H264_FRAME_FILES          90
#define NUMBER_OF_AAC_FRAME_FILES           299

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
    CHAR sampleDir[MAX_PATH_LEN + 1];
    FrameData audioFrames;
    FrameData videoFrames;
} SampleCustomData, *PSampleCustomData;

/* Flag set by '--x509' */
static int x509_flag = 0;

static struct option long_options[] = {
    /*   NAME       ARGUMENT           FLAG     SHORTNAME */
    {"channel-name",    required_argument,       NULL, 'n'},
    {"duration",    required_argument,       NULL, 'd'},
    {"dir",         required_argument,       NULL, 'D'},
    {"size",        required_argument,       NULL, 's'},
    {"x509",        no_argument,       &x509_flag, 1},
    {"help",        no_argument,       NULL, 'h'},
    {NULL,      0,                 NULL, 0}
};

void display_usage( int err )
{
    printf ("Usage: KinesisVideoProducerApp -n <channel-name> [OPTION]...\n");
    printf ("Ingest video to the Amazon Kinesis Video Streams service.\n");
    printf ("\n");
    printf ("Mandatory arguments to long options are mandatory for short options too.\n");
    printf ("-n, --channel-name     stream channel name\n");
    printf ("-d, --directory        streaming media directory\n");
    printf ("                       default to '../'\n");
    printf ("-D, --duration         streaming duration in second\n");
    printf ("                       default to 600\n");
    printf ("-s, --size             stream buffer size in KB\n");
    printf ("                       default to 2048\n");
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
    PCHAR accessKey = NULL, secretKey = NULL, sessionToken = NULL, streamName = NULL, region = NULL, cacertPath = NULL;
    UINT64 streamStopTime,  fileSize = 0;
    TID audioSendTid, videoSendTid;
    SampleCustomData data;
    UINT32 i;
    PTrackInfo pAudioTrack = NULL;
    BYTE audioCpd[KVS_AAC_CPD_SIZE_BYTE];

    CHAR filePath[MAX_PATH_LEN + 1];
    MEMSET(&data, 0x00, SIZEOF(SampleCustomData));

    if (argc < 3) {
        display_usage(1);
        CHK(FALSE, STATUS_INVALID_ARG);
    }

    if ((accessKey = getenv(ACCESS_KEY_ENV_VAR)) == NULL || (secretKey = getenv(SECRET_KEY_ENV_VAR)) == NULL) {
        printf("Error missing credentials\n");
        CHK(FALSE, STATUS_INVALID_ARG);
    }

    int choice;
    int *channel_name = 0;
    int *media_dir = "../";
    UINT64 streamingDuration = DEFAULT_STREAM_DURATION, bufferSize = DEFAULT_STORAGE_SIZE;
    int option_index = 0;

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
            channel_name = optarg;
            printf ("KVS channel name is '%s'\n", channel_name);
            break;
        case 'd':
            media_dir = optarg;
            printf ("KVS stream media from '%s'\n", media_dir);
            break;
        case 'D':
            CHK_STATUS(STRTOUI64(optarg, NULL, 10, &streamingDuration));
            printf ("KVS streaming for %d seconds\n", streamingDuration);
            break;
        case 's':
            CHK_STATUS(STRTOUI64(optarg, NULL, 10, &bufferSize));
            bufferSize *= 1024;
            printf ("KVS video buffer size is %d KB\n", bufferSize);
            break;
        case 'h':
            display_usage(0);
            break;
        case ':':
        /* missing option argument */
        fprintf(stderr, "%s: option '-%c' requires an argument\n",
                argv[0], optopt);
            display_usage(1);
            break;
        case '?':
            break;
        default:
            printf ("?? getopt returned character code 0%o ??\n", choice);
        }
    }
    /* we report the final status resulting. */
    if (x509_flag)
        printf ("X.509 flag is %d \n", x509_flag);

    if (optind < argc) {
        printf ("non-option ARGV-elements: ");
        while (optind < argc)
            printf ("%s ", argv[optind++]);
        printf ("\n");
    }
    
    MEMSET(data.sampleDir, 0x00, MAX_PATH_LEN + 1);
    STRNCPY(data.sampleDir, media_dir, MAX_PATH_LEN);
    if (data.sampleDir[STRLEN(data.sampleDir) - 1] == '/') {
        data.sampleDir[STRLEN(data.sampleDir) - 1] = '\0';
    }

    cacertPath = getenv(CACERT_PATH_ENV_VAR);
    sessionToken = getenv(SESSION_TOKEN_ENV_VAR);
    streamName = channel_name;
    if ((region = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
        region = (PCHAR) DEFAULT_AWS_REGION;
    }

    // Get the duration and convert to an integer
    streamStopTime = defaultGetTime() + streamingDuration*HUNDREDS_OF_NANOS_IN_A_SECOND;

    // default storage size is 128MB. Use setDeviceInfoStorageSize after create to change storage size.
    CHK_STATUS(createDefaultDeviceInfo(&pDeviceInfo));
    // adjust members of pDeviceInfo here if needed
    pDeviceInfo->clientInfo.loggerLogLevel = LOG_LEVEL_DEBUG;

    // must larger than MIN_STORAGE_ALLOCATION_SIZE
    pDeviceInfo->storageInfo.storageSize = bufferSize > MIN_STORAGE_ALLOCATION_SIZE ?
                                           bufferSize : 
                                           MIN_STORAGE_ALLOCATION_SIZE;


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
