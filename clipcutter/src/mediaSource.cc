#include "pch.h"
#include "mediaSource.h"
#include "app.h"
#include "playback.h"
#include "errors.h"

 char* GetFileNameFromPath(char* _buffer) {
	char c;
	int  i;
	for (i = 0; ; ++i) {
		c = *((char*)_buffer + i);
		if (c == '\\' || c == '/')
			return GetFileNameFromPath((char*)_buffer + i + 1);
		if (c == '\0')
			return _buffer;
	}
	return nullptr;
}

static bool ensureScratchCapacity(float** scratch, size_t* capacity, int neededSamples) {
    if (neededSamples < 0) return false; // swr_get_out_samples() returns negative values on errors.
    if ((size_t)neededSamples <= *capacity) return true;

    // apperently audio frame sizes are quite stable, so this probably won't be called much at all.
    float* newScratch = (float*) realloc(*scratch, (size_t)neededSamples * sizeof(float));
    if (!newScratch) return false;

    *scratch = newScratch;
    *capacity = (size_t)neededSamples;
    return true;
}

static void accumulatePeaks(const float* samples, int n,
                              float* accumMin, float* accumMax, int* accumCount,
                              int blockSize, DynArr* peaks) {
    for (int s = 0; s < n; s++) {
        float v = samples[s];
        if (*accumCount == 0) {
            *accumMin = *accumMax = v;
        } else {
            *accumMin = minf(*accumMin, v);
            *accumMax = maxf(*accumMax, v);
        }
        if (++(*accumCount) >= blockSize) {
            PeakBlock block = { *accumMin, *accumMax };
            DynArr_Append(peaks, &block);
            *accumCount = 0;
        }
    }
}

static bool resample_into_peaks(SwrContext* swrCtx, const uint8_t** inData, int inSamples,
                                 float** scratch, size_t* scratchCapacity,
                                 float* accumMin, float* accumMax, int* accumCount,
                                 int blockSize, DynArr* peaks) {
    int maxOut = swr_get_out_samples(swrCtx, inSamples);
    if (maxOut == 0) return true;
    if (!ensureScratchCapacity(scratch, scratchCapacity, maxOut)) return false;

    uint8_t* outPtrs[1] = { (uint8_t*) *scratch };
    int n = swr_convert(swrCtx, outPtrs, maxOut, inData, inSamples);
    if (n > 0) {
        accumulatePeaks(*scratch, n, accumMin, accumMax, accumCount, blockSize, peaks);
    }
    return true;
}



CC_FFmpegError* getPeakBlocks(MediaSource* mediaSource, AVFormatContext* ifmt_ctx) {
    const int blockSize = PEAK_BLOCK_SIZE;
    int ret = 0;
    char* err = nullptr;

    int audioStreamCount = 0;
    int audioStreamIds[MAX_SUPPORTED_AUDIO_TRACKS];

    AVCodecContext* audioDecCtx[MAX_SUPPORTED_AUDIO_TRACKS];
    SwrContext* swrCtx[MAX_SUPPORTED_AUDIO_TRACKS] = {0};
    DynArr trackPeaks[MAX_SUPPORTED_AUDIO_TRACKS];
    int accumCount[MAX_SUPPORTED_AUDIO_TRACKS] = {0};
    float accumMin[MAX_SUPPORTED_AUDIO_TRACKS] = {0};
    float accumMax[MAX_SUPPORTED_AUDIO_TRACKS] = {0};
    int tracksInitialized = 0;

    AVPacket* pkt = NULL;
    AVFrame*  frame = NULL;
    float* scratch = NULL;
    size_t scratchCapacity = 0;



    for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *inStream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = inStream->codecpar;

        if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIds[audioStreamCount++] = i;
        }
    }

    if (audioStreamCount == 0) {
        return nullptr;
    }


   //────────────────── get decoders + resamplers ──────────────────

   for (int i=0; i < audioStreamCount; i++) {
        const AVStream* stream = ifmt_ctx->streams[audioStreamIds[i]];
        const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!decoder) {
            err = alloc_error("Failed to find audio decoder from audio stream");
            goto cleanup;
        }

        audioDecCtx[i] = avcodec_alloc_context3(decoder);
        if (!audioDecCtx[i]) {
            err = alloc_error("Failed to allocate audio decoder context");
            goto cleanup;
        }

        ret = avcodec_parameters_to_context(audioDecCtx[i], stream->codecpar);
        if (ret < 0) {
            err = alloc_error("Failed to copy codec parameters to audio decoder");
            goto cleanup;
        }

        ret = avcodec_open2(audioDecCtx[i], decoder, NULL);
        if (ret < 0) {
            err = alloc_error("Failed to open audio codec");
            goto cleanup;
        }

        mediaSource->sampleRates[i] = audioDecCtx[i]->sample_rate;

        // we normalize the sources to mono float so we don't have
        // to write code to accomendate for different sample formats since that
        // varies between codecs. 
        AVChannelLayout monoLayout;
        av_channel_layout_default(&monoLayout, 1);
        swr_alloc_set_opts2(&swrCtx[i],
            &monoLayout, AV_SAMPLE_FMT_FLT, audioDecCtx[i]->sample_rate,
            &audioDecCtx[i]->ch_layout, audioDecCtx[i]->sample_fmt, audioDecCtx[i]->sample_rate,
            0, nullptr);
        swr_init(swrCtx[i]);


        // estimate size of PeakBlocks
        int64_t estBlocks = 4096; // fallback
        if (stream->duration != AV_NOPTS_VALUE) {
            double durationSec = stream->duration * av_q2d(stream->time_base);
            int64_t estSamples = (int64_t)(durationSec * audioDecCtx[i]->sample_rate);
            estBlocks = estSamples / blockSize + 1;
        }
        DynArr_Init(&trackPeaks[i], sizeof(PeakBlock), (size_t) estBlocks);

        tracksInitialized = i + 1;

    }

    //──────────────── prepare decoding ────────────────

    pkt = av_packet_alloc();
    if (!pkt) {
        err = alloc_error("Could not allocate AVPacket");
        goto cleanup;
    }
    frame = av_frame_alloc();
    if (!frame) {
        err = alloc_error("Failed to allocate AVFrame");
        goto cleanup;
    }
    //───────────────── decoding loop ──────────────────

    while (1) {
        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret < 0) // end of file
            break;

        int in_index = pkt->stream_index;
        AVStream *in_stream = ifmt_ctx->streams[in_index];

        if (in_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            av_packet_unref(pkt);
            continue;
        }

        int idx = -1;
        for (int i=0; i < audioStreamCount; i++) {
            if (in_index == audioStreamIds[i]) {
                idx = i;
                break;
            }
        }

        // skip if we couldn't find the audioStreamId based on the streamId.
        if (idx == -1) {
            av_packet_unref(pkt);
            continue; 
        }

        ret = avcodec_send_packet(audioDecCtx[idx], pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            err = alloc_error("Failed to send packet to decoder");
            goto cleanup;
        }
        while (ret >= 0) {
            ret = avcodec_receive_frame(audioDecCtx[idx], frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0) {
                err = alloc_error("Error during audio decoding");
                goto cleanup;
            }
            if (!resample_into_peaks(swrCtx[idx], (const uint8_t**) frame->data, frame->nb_samples,
                                      &scratch, &scratchCapacity,
                                      &accumMin[idx], &accumMax[idx], &accumCount[idx],
                                      blockSize, &trackPeaks[idx])) {
                err = alloc_error("scratch realloc failed");
                goto cleanup;
            }
        }
    }

    //───────── flush decordes and resamplers ──────────
    for (int i = 0; i < audioStreamCount; i++) {
        avcodec_send_packet(audioDecCtx[i], NULL);
        while (avcodec_receive_frame(audioDecCtx[i], frame) == 0) {
            if (!resample_into_peaks(swrCtx[i], (const uint8_t**) frame->data, frame->nb_samples,
                                      &scratch, &scratchCapacity,
                                      &accumMin[i], &accumMax[i], &accumCount[i],
                                      blockSize, &trackPeaks[i])) {
                err = alloc_error("scratch realloc failed");
                goto cleanup;
            }
        }

        if (!resample_into_peaks(swrCtx[i], NULL, 0,
                                  &scratch, &scratchCapacity,
                                  &accumMin[i], &accumMax[i], &accumCount[i],
                                  blockSize, &trackPeaks[i])) {
            err = alloc_error("scratch realloc failed");
            goto cleanup;
        }

        if (accumCount[i] > 0) {
            PeakBlock block = { accumMin[i], accumMax[i] };
            DynArr_Append(&trackPeaks[i], &block);
        } 
    }

    //───────────────── update results ─────────────────
    mediaSource->peakBlocks = (DynArr*) malloc(audioStreamCount * sizeof(DynArr));
    for (int i = 0; i < audioStreamCount; i++) {
        mediaSource->peakBlocks[i] = trackPeaks[i]; //shallow struct copy, keep in mind
    }

    // I could realloc the buffers so that capacity is not greater than their size.
cleanup:
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (scratch) free(scratch);
    if (ifmt_ctx)
        avformat_close_input(&ifmt_ctx);

    for (int i = 0; i < tracksInitialized; i++) {
        if (audioDecCtx[i]) avcodec_free_context(&audioDecCtx[i]);
        if (swrCtx[i]) swr_free(&swrCtx[i]);
        if (err && trackPeaks[i].items) DynArr_Free(&trackPeaks[i]);
    }

    if (err) {
        CC_FFmpegError* exportErr = (CC_FFmpegError*) malloc(sizeof(CC_FFmpegError));
        exportErr->message = err;
        exportErr->FFmpegError = nullptr;

        if (ret < 0 && ret != AVERROR_EOF) {
            char* errbuf = (char*) malloc(512);
            av_strerror(ret, errbuf, 512);
            exportErr->FFmpegError = errbuf;
        }

        if (exportErr) {
            char* errBuff = (char*) malloc(512);
            sprintf(errBuff, "Generating audio peaks failed.\n%s\nError: %s", exportErr->message, exportErr->FFmpegError);
            log_error(errBuff);
            popup_error("Audio peak generation failed", errBuff);
        }
        log_info("Finished generating peak blocks.")
    }


    mediaSource->peaksGenerated = true;
    return nullptr;
}


void MediaSource_Init(App* app, MediaSource** mediaSourceP, const char* path) {
    cc_unused(app);
    MediaSource* mediaSource = *mediaSourceP;
	memset(mediaSource, 0, sizeof(MediaSource));

	mediaSource->filename = nullptr;
	mediaSource->path = strdup(path);
	mediaSource->filename = strdup(GetFileNameFromPath((char*)path));
	if (mediaSource->filename == nullptr) {
		log_fatal("Failed to get filename from path");
		App_Die();
	}

	char* url = (char*) malloc(strlen(path) + sizeof("file:") + 1);
	sprintf(url, "file:%s", path);

	AVFormatContext* ifmt_ctx = NULL;
	int ret = avformat_open_input(&ifmt_ctx, url, NULL, NULL);
	if (ret < 0) {
		log_fatal("ffmpeg failed to retrieve information about video source");
		App_Die();
	}

	avformat_find_stream_info(ifmt_ctx, nullptr);
	log_debug("streams:%d", ifmt_ctx->nb_streams);
	log_debug("duration:%.2f", ifmt_ctx->duration/AV_TIME_BASE);

	mediaSource->audioTracks = 0;
    for (unsigned int i=0; i < ifmt_ctx->nb_streams; i++) {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            mediaSource->audioTracks++;
        }
    }

    if (mediaSource->audioTracks > MAX_SUPPORTED_AUDIO_TRACKS) {
        log_error("Cannot initialize a media source with more than %d audiotracks.", MAX_SUPPORTED_AUDIO_TRACKS);
        free(mediaSource);
        *mediaSourceP = nullptr;
        avformat_close_input(&ifmt_ctx);
        return;
    }

    mediaSource->length = (float) ifmt_ctx->duration / AV_TIME_BASE;


    mediaSource->peaksGenerated = false;
    log_info("Starting peak block generation.")
    // peakBlocks is responsible for closing the input file
    std::thread thread_obj(getPeakBlocks, mediaSource, ifmt_ctx);
    thread_obj.detach();

}

void MediaSource_Free(MediaSource* source) {

    if (source->filename != nullptr) {
        free(source->filename);
    }
    if (source->path != nullptr) {
        free(source->path);
    }

    if (source->peaksGenerated) {
        for (int i=0; i < source->audioTracks; i++) {
            DynArr_Free(&source->peakBlocks[i]);
        }
    }
}

void MediaSource_Load(App* app, MediaSource* source, float startTime) {
    log_trace("MediaSource_Load()");
	app->playbackBlocked = true;
	app->isLoadingVideo = true;
	app->loadedMediaSource = source;
	Playback_LoadVideo(app, source->path, startTime);
    Playback_ApplyLavfiComplex(app);
}
