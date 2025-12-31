#include "export.h"
#include "pch.h"
#include "app.h"
#include "mediaSource.h"
#include "mediaClip.h"
#include <SDL3/SDL_messagebox.h>

#define MAX_ERROR_LENGTH 4096
const char* const ENCODER_PRESETS[] = {
    "ultrafast",
    "superfast", 
    "veryfast", 
    "faster", 
    "fast", 
    "medium", 
    "slow", 
    "slower", 
    "veryslow"
};
const int ENCODER_PRESET_COUNT = sizeof(ENCODER_PRESETS) / sizeof(ENCODER_PRESETS[0]);

char* alloc_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* errorBuffer = (char*) malloc(MAX_ERROR_LENGTH);
    vsnprintf(errorBuffer, MAX_ERROR_LENGTH, fmt, args);
    va_end(args);

    log_error(errorBuffer);
    return errorBuffer;
}

void Export_SetDefaultExportOptionsVideo(App* app) {
    app->exportState.exportOptions.exportVideoSelected = true;
    app->exportState.exportOptions.exportAudio = true;
    app->exportState.exportOptions.mergeAudioTracks = true;
    app->exportState.exportOptions.CBRRateFactor = 23.0;
    app->exportState.exportOptions.encoderPresetIndex = 5; // medium
}

void Export_SetDefaultExportOptionsAudio(App* app) {
    app->exportState.exportOptions.exportVideoSelected = false;
    app->exportState.exportOptions.exportAudio = true;
    app->exportState.exportOptions.mergeAudioTracks = true;
}

char* setOutputParameters(MediaClip* firstClip, ExportState* exportState) {
    AVFormatContext* ofmt_ctx = exportState->ofmt_ctx;
    AVFormatContext *ifmt_ctx = NULL;
    int audioStreamIdx[MAX_SUPPORTED_AUDIO_TRACKS];
    char* err = nullptr;
    int ret;
    int inVideoStreamIdx = -1;
    
    if ((ret = avformat_open_input(&ifmt_ctx, firstClip->source->path, 0, 0)) < 0) {
        err = alloc_error("Could not open input file '%s'", firstClip->source->path);
        goto cleanup;
    }
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        err = alloc_error("Failed to retrieve input stream information");
        goto cleanup;
    }

    log_info("stealing output parameters from the following video file:");
    av_dump_format(ifmt_ctx, 0, firstClip->source->path, 0);


    { // find audio and video track
        int audioStreamCount = 0;
        log_debug("nb sttream is: %d", ifmt_ctx->nb_streams);
        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
            AVStream *inStream = ifmt_ctx->streams[i];
            AVCodecParameters *in_codecpar = inStream->codecpar;

            if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (inVideoStreamIdx != -1) {
                    err = alloc_error("Found more than two video streams");
                    goto cleanup;
                }
                inVideoStreamIdx = i;
            } else if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                log_debug("found audio");
                audioStreamIdx[audioStreamCount++] = i;
            }
        }

        if (inVideoStreamIdx == -1) {
            err = alloc_error("Found no video source");
            goto cleanup;
        }

    }

    ret = avcodec_parameters_copy(exportState->out_video_stream->codecpar, ifmt_ctx->streams[inVideoStreamIdx]->codecpar);
    if (ret < 0) {
        err = alloc_error("Failed to copy video codec parameters");
        goto cleanup;
    }

    ret = avcodec_parameters_copy(exportState->out_audio_stream->codecpar, ifmt_ctx->streams[audioStreamIdx[0]]->codecpar);
    if (ret < 0) {
        err = alloc_error("Failed to copy audio codec parameters");
        goto cleanup;
    }

    av_dump_format(ofmt_ctx, 0, exportState->out_filename, 1);

    // open output file for writing
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, exportState->out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            err = alloc_error("Could not open output file '%s'", exportState->out_filename);
            goto cleanup;
        }
    }



cleanup:
    avformat_close_input(&ifmt_ctx);

    log_error("setting output parameters failed with error as %d", err);
    return err;
}

char* remuxClip(MediaClip* mediaClip, ExportState* exportState);

char* remuxMultipleClips(MediaClip** mediaClips, ExportState* exportState, const char* out_filename) {
    AVFormatContext *ofmt_ctx = NULL;
    char* err = nullptr;
    exportState->offsetPtsEncTBVideo = 0; 
    exportState->offsetPtsEncTBAudio = 0; 
    exportState->lastPtsEncTBVideo = AV_NOPTS_VALUE;
    exportState->lastPtsEncTBAudio = AV_NOPTS_VALUE;
    exportState->out_filename = out_filename;
    
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, exportState->out_filename);
    if (!ofmt_ctx) {
        err = alloc_error("Could not create output context");
        return err;
    }
    exportState->ofmt_ctx = ofmt_ctx;

    exportState->out_video_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!exportState->out_video_stream) {
        err = alloc_error("Failed allocating video output stream");
        goto cleanup;
    }

    exportState->out_audio_stream = avformat_new_stream(ofmt_ctx, NULL);
    if (!exportState->out_audio_stream) {
        err = alloc_error("Failed allocating audio output stream");
        goto cleanup;
    }

    err = setOutputParameters(mediaClips[0], exportState);
    if (err)
        goto cleanup;

    for (int i=0; i < MEDIACLIPS_SIZE; i++) {
        // if (i==1) break;
        MediaClip* mediaClip = mediaClips[i];
        if (mediaClip == nullptr) break;
        if (exportState->lastPtsEncTBVideo != AV_NOPTS_VALUE) {
            // +1 since we want to write the frame after the last one.
            exportState->offsetPtsEncTBVideo = exportState->lastPtsEncTBVideo+1; 
            exportState->offsetPtsEncTBAudio = exportState->lastPtsEncTBAudio+1; 
        }

        exportState->clipIndex = i;
        log_debug("appending mediaClip with index %d", i);
        char* statusStrPtr = (char*) malloc(strlen(mediaClip->source->filename) + sizeof("Processing ") + 1);
        sprintf(statusStrPtr, "Processing %s", mediaClip->source->filename);
        exportState->statusString = statusStrPtr;
        err = remuxClip(mediaClip, exportState);
        if (err) {
            goto cleanup;
        }
        exportState->statusString = nullptr;
        free(statusStrPtr);
    }

    { // output file
        av_write_trailer(ofmt_ctx);

    }

cleanup:
    // close output
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    log_debug("finished remuxing with errored as: %d", err);

    return err;

};




// process any frames that ffmpeg has finished decoding
// returns nullptr on success, else err message pointer
char* recieveAndProcessFramesVideo(
        ExportState* exportState,
        AVFormatContext *ifmt_ctx,
        AVFormatContext* ofmt_ctx,
        AVFrame* frame,
        int64_t start_TS,
        int64_t end_TS,
        AVCodecContext* videoDecCtx,
        AVCodecContext* videoEncCtx,
        int in_index,
        bool* isPastEndTSVideo,
        bool decoderIsBeingDrained,
        int found_rebase_pts[MAX_SUPPORTED_AUDIO_TRACKS+1],
        int64_t pts_offset[MAX_SUPPORTED_AUDIO_TRACKS+1]
        ) {

    cc_unused(decoderIsBeingDrained);
    int64_t* last_video_pts_enc_tb = &exportState->lastPtsEncTBVideo;
    AVStream* in_stream = ifmt_ctx->streams[in_index];

    while (1) {
        int ret = avcodec_receive_frame(videoDecCtx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }

        frame->pts = av_rescale_q(frame->pts,
                                in_stream->time_base,
                                videoEncCtx->time_base);

        int64_t pts_us_frame = av_rescale_q(frame->pts,
                                            videoEncCtx->time_base,
                                            AV_TIME_BASE_Q);

        // TODO: check if this is really necessary.
        frame->pts = frame->best_effort_timestamp;
        if (frame->pts == AV_NOPTS_VALUE) {
            frame->pts = *last_video_pts_enc_tb++;
            *last_video_pts_enc_tb = frame->pts;
        }

        // drop packet if it's before our span
        if (pts_us_frame < start_TS) {
            av_frame_unref(frame);
            continue;
        }

        // if after trimming region, mark this stream as finished
        if (pts_us_frame > end_TS) {
            *isPastEndTSVideo = true;
            av_frame_unref(frame);
            continue;
        }

        if ((*exportState->audioStreamDisabled)[in_index]) {
            continue;
        }

        // calculate pts_offset if needed
        if (!found_rebase_pts[in_index]) {
            pts_offset[in_index] = frame->pts; // frame->pts is in enc TB
            found_rebase_pts[in_index] = true;
        }

        // rebase timestamp
        int64_t rebased_pts = frame->pts - pts_offset[in_index] + exportState->offsetPtsEncTBVideo;

        // ensure monotically increasing pts
        int64_t last_pts = *last_video_pts_enc_tb;
        // TODO: check if the AV_NOPTS_VALUE check here is redundant.
        if (last_pts != AV_NOPTS_VALUE && rebased_pts <= last_pts) {
            log_debug("non monotically increasing pts for video, fixing")
            rebased_pts= last_pts + 1;
        }
        *last_video_pts_enc_tb = rebased_pts;

        frame->pts = rebased_pts;

        int ret_send_frame = avcodec_send_frame(videoEncCtx, frame);
        if (ret_send_frame < 0 && ret_send_frame != AVERROR(EAGAIN)) {
            return alloc_error("Error sending frame to encoder");
        }


        AVPacket *enc_pkt = av_packet_alloc();
        while (1) {
            int er = avcodec_receive_packet(videoEncCtx, enc_pkt);
            if (er == AVERROR(EAGAIN) || er == AVERROR_EOF) break;

            av_packet_rescale_ts(enc_pkt, videoEncCtx->time_base, exportState->out_video_stream->time_base);
            enc_pkt->stream_index = 0;

            ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
            if (ret < 0) {
                av_packet_free(&enc_pkt);
                av_frame_unref(frame);
                return alloc_error("Error writing re-encoded video frame");
            }

            av_packet_unref(enc_pkt);
        }
        av_packet_free(&enc_pkt);
        av_frame_unref(frame);

    }

    return nullptr;
}

// returns pointer to error message or nullptr if successful
char* remuxClip(MediaClip* mediaClip, ExportState* exportState) {
    const char* in_filename = mediaClip->source->path;
    AVFormatContext* ofmt_ctx = exportState->ofmt_ctx;
    AVPacket* pkt = NULL;
    AVFrame* frame = NULL;
    AVFormatContext *ifmt_ctx = NULL;
    AVFilterContext* abufferCtxs[MAX_SUPPORTED_AUDIO_TRACKS] = { NULL};
    AVFilterContext* abuffersink_ctx;
    AVCodecContext* videoDecCtx;
    AVCodecContext* audioDecCtx[MAX_SUPPORTED_AUDIO_TRACKS];
    AVCodecContext *audioEncCtx;
    AVCodec* audioEnc;
    AVCodecContext *videoEncCtx;
    AVCodec* videoEnc;
    memset(audioDecCtx, NULL, sizeof(audioDecCtx));
    int inVideoStreamIdx = -1;
    int outAudioStreamIdx = -1;
    int enabledAudioStreamIdx[MAX_SUPPORTED_AUDIO_TRACKS];
    memset(enabledAudioStreamIdx, -1, sizeof(enabledAudioStreamIdx));
    int enabledAudioStreamCount = 0;
    // int64_t streamRescaledStartSeconds[MAX_SUPPORTED_AUDIO_TRACKS+1];
    // int64_t streamRescaledEndSeconds[MAX_SUPPORTED_AUDIO_TRACKS+1];
    int ret;
    char* err = nullptr;
    // int64_t last_audio_dts;

    int found_rebase_pts[MAX_SUPPORTED_AUDIO_TRACKS+1];
    int64_t pts_offset[MAX_SUPPORTED_AUDIO_TRACKS+1];

    for (int i = 0; i < MAX_SUPPORTED_AUDIO_TRACKS+1; i++) {
        pts_offset[i] = 0;
        found_rebase_pts[i] = false;
    }



    int64_t start_TS = mediaClip->startCutoff * AV_TIME_BASE;
    int64_t end_TS = (mediaClip->source->length-mediaClip->endCutoff) * AV_TIME_BASE;


    const char* out_filename = exportState->out_filename;
    log_debug("filename is: %s", out_filename);
    AVStream* out_audio_stream  = exportState->out_audio_stream;
    int64_t* last_audio_pts_enc_tb = &exportState->lastPtsEncTBAudio;


    AVFilterGraph* filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        return alloc_error("Unable to create filter graph");
    }

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        err = alloc_error("Could not open input file '%s'", in_filename);
        goto cleanup;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        err = alloc_error("Failed to retrieve input stream information");
        goto cleanup;
    }



    {
        inVideoStreamIdx = -1;
        log_debug("modified nb sttream is: %d", ifmt_ctx->nb_streams);
        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
            AVStream *inStream = ifmt_ctx->streams[i];
            AVCodecParameters *in_codecpar = inStream->codecpar;

            // streamRescaledStartSeconds[i] = av_rescale_q(mediaClip->startCutoff * AV_TIME_BASE, AV_TIME_BASE_Q, inStream->time_base);
            // streamRescaledEndSeconds[i] = av_rescale_q((mediaClip->source->length-mediaClip->endCutoff) * AV_TIME_BASE, AV_TIME_BASE_Q, inStream->time_base);

            if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
                in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
                continue;
            }

            if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                log_debug("found video");
                if (inVideoStreamIdx != -1) {
                    err = alloc_error("Found more than two video streams");
                    goto cleanup;
                }

                inVideoStreamIdx = i;
            } else if (in_codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                log_debug("found audio at stream index %d", i);
                if (!(*exportState->audioStreamDisabled)[i]) {
                    enabledAudioStreamIdx[enabledAudioStreamCount++] = i;
                } else {
                    log_debug("ignoring the audio track as it is disabled.");
                }
            }
        }

        if (inVideoStreamIdx == -1) {
            err = alloc_error("Found no video source");
            goto cleanup;
        }


        AVCodecParameters* outAudioCodecPar = exportState->out_audio_stream->codecpar;


        outAudioStreamIdx = out_audio_stream->index;

        { // get video decoder
            const AVStream* vidStream = ifmt_ctx->streams[inVideoStreamIdx];
            const AVCodec* decoder = avcodec_find_decoder(vidStream->codecpar->codec_id);
            if (!decoder) {
                err = alloc_error("Failed to find video decoder from video stream");
                goto cleanup;
            }

            videoDecCtx = avcodec_alloc_context3(decoder);
            if (!videoDecCtx) {
                err = alloc_error("Failed to allocate video decoder context");
                goto cleanup;
            }

            ret = avcodec_parameters_to_context(videoDecCtx, vidStream->codecpar);
            if (ret < 0) {
                err = alloc_error("Failed to copy codec parameters to video decoder");
                goto cleanup;
            }

            ret = avcodec_open2(videoDecCtx, decoder, NULL);
            if (ret < 0) {
                err = alloc_error("Failed to open video codec");
                goto cleanup;
            }
        }

        { // get decoders from audio streams
            for (int i=0; i < enabledAudioStreamCount; i++) {
                const AVStream* stream = ifmt_ctx->streams[enabledAudioStreamIdx[i]];
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

                // an abuffer lets us feed audio data into a filter
                const AVFilter* abuffer = avfilter_get_by_name("abuffer");
                if (!abuffer) {
                    err = alloc_error("Could not find the abuffer filter");
                    goto cleanup;
                }


                AVFilterContext* abuffer_ctx = avfilter_graph_alloc_filter(filter_graph, abuffer, "src");
                abufferCtxs[i] = abuffer_ctx;
                if (!abuffer_ctx) {
                    err = alloc_error("Could not allocate the abuffer instance.");
                    goto cleanup;
                }

                char ch_layout[64];
                av_channel_layout_describe((const AVChannelLayout*)&stream->codecpar->ch_layout, ch_layout, sizeof(ch_layout));
                av_opt_set    (abuffer_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
                av_opt_set    (abuffer_ctx, "sample_fmt", av_get_sample_fmt_name((AVSampleFormat)stream->codecpar->format), AV_OPT_SEARCH_CHILDREN);
                av_opt_set_q  (abuffer_ctx, "time_base", stream->time_base, AV_OPT_SEARCH_CHILDREN);
                av_opt_set_int(abuffer_ctx, "sample_rate", stream->codecpar->sample_rate, AV_OPT_SEARCH_CHILDREN);


                /* Now initialize the filter; we pass NULL options, since we have already
                * set all the options above. */
                ret = avfilter_init_str(abuffer_ctx, NULL);
                if (ret < 0) {
                    err = alloc_error("Could not initialize the abuffer filter");
                    goto cleanup;
                }
            }
        }

        if (enabledAudioStreamCount == 0) {
            log_debug("No audio tracks. only exporting video.");
        } else {

            const AVFilter* amix = avfilter_get_by_name("amix");
            if (!amix) {
                err = alloc_error("Could not find the amix filter");
                goto cleanup;
            }

            AVFilterContext* amix_ctx = avfilter_graph_alloc_filter(filter_graph, amix, "src");
            if (!amix_ctx) {
                err = alloc_error("Could not allocate the amix filter context");
                goto cleanup;
            }

            av_opt_set_int(amix_ctx, "inputs", enabledAudioStreamCount, AV_OPT_SEARCH_CHILDREN);

            // NULL since we set the options with av_opt above.
            ret = avfilter_init_str(amix_ctx, NULL);
            if (ret < 0) {
                err = alloc_error("Could not initialize the amix filter");
                goto cleanup;
            }

            const AVFilter* aformat = avfilter_get_by_name("aformat");
            if (!aformat) {
                err = alloc_error("Could not find the aformat filter");
                goto cleanup;
            }

            AVFilterContext* aformat_ctx = avfilter_graph_alloc_filter(filter_graph, aformat, "aformat");
            if (!aformat_ctx) {
                err = alloc_error("Could not allocate the aformat instance");
                goto cleanup;
            }

            char options_str[1024];
            snprintf(options_str, sizeof(options_str),
                    "sample_fmts=%s:sample_rates=%d:channel_layouts=stereo",
                    av_get_sample_fmt_name((AVSampleFormat)outAudioCodecPar->format), outAudioCodecPar->sample_rate);
            ret = avfilter_init_str(aformat_ctx, options_str);
            if (ret < 0) {
                err = alloc_error("Could not initialize the aformat filter");
                goto cleanup;
            }
            
            // TODO: add custom effects here
            // test with modified compressor
            // const char* acompressor_desc = "attack=26.85600:release=664.43903:ratio=20.00000:threshold=0.04800:level_in=11.61000:makeup=1.00000";
            // test with default settings (no compressor)
            const char* acompressor_desc = "attack=20.00000:release=250.00000:ratio=2.00000:threshold=0.12500:level_in=1.00000:makeup=1.00000";

            const AVFilter* acompressor = avfilter_get_by_name("acompressor");
            AVFilterContext* acompressor_ctx = NULL;

            ret = avfilter_graph_create_filter(&acompressor_ctx, acompressor, "acompressor", acompressor_desc, NULL, filter_graph);
            if (ret < 0) {
                err = alloc_error("Failed adding user effect");
                goto cleanup;
            }


            const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
            if (!abuffersink) {
                err = alloc_error("Could not find the abuffersink filter");
                goto cleanup;
            }

            abuffersink_ctx = avfilter_graph_alloc_filter(filter_graph, abuffersink, "sink");
            if (!abuffersink_ctx) {
                err = alloc_error("Could not allocate the abuffersink instance");
                goto cleanup;
            }

            // Set the output format constraints to match the encoder
            AVSampleFormat sample_fmts[] = { (AVSampleFormat)outAudioCodecPar->format, AV_SAMPLE_FMT_NONE };
            ret = av_opt_set_int_list(abuffersink_ctx, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
            if (ret < 0) {
                err = alloc_error("Could not set output sample format");
                goto cleanup;
            }

            int sample_rates[] = { outAudioCodecPar->sample_rate, -1 };
            ret = av_opt_set_int_list(abuffersink_ctx, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN);
            if (ret < 0) {
                err = alloc_error("Could not set output sample rate");
                goto cleanup;
            }



            /* This filter takes no options. */
            ret = avfilter_init_str(abuffersink_ctx, NULL);
            if (ret < 0) {
                err = alloc_error("Could not initialize the abuffersink instance.");
                goto cleanup;
            }

            for (int i=0; i < enabledAudioStreamCount; i++) {
                if (ret >= 0)
                    ret = avfilter_link(abufferCtxs[i], 0, amix_ctx, i);
            }
            if (ret >= 0)
                ret = avfilter_link(amix_ctx, 0, aformat_ctx, 0);
            if (ret >= 0)
                ret = avfilter_link(aformat_ctx, 0, acompressor_ctx, 0);
            if (ret >= 0)
                ret = avfilter_link(acompressor_ctx, 0, abuffersink_ctx, 0);
            if (ret < 0) {
                err = alloc_error("Error connecting filters");
                goto cleanup;
            }


            ret = avfilter_graph_config(filter_graph, NULL);
            if (ret < 0) {
                err = alloc_error("Error configuring the filter graph");
                goto cleanup;
            }

        }


        // create audio encoder
        if (enabledAudioStreamCount != 0) {
            audioEnc = (AVCodec*) avcodec_find_encoder(ofmt_ctx->oformat->audio_codec);            

            if (!audioEnc) {
                err = alloc_error("Necessary audio encoder not found");
                goto cleanup;
            }

            audioEncCtx = avcodec_alloc_context3(audioEnc);

            if (!audioEncCtx) {
                err = alloc_error("Failed to allocate audio encoder context");
                goto cleanup;
            }

            audioEncCtx->sample_rate = audioDecCtx[0]->sample_rate;
            /*av_channel_layout_default(&enc_ctx->ch_layout, 2); // stereo layout*/
            av_channel_layout_default(&audioEncCtx->ch_layout, outAudioCodecPar->ch_layout.nb_channels); // stereo layout
            /*enc_ctx->bit_rate = 320000;  // 320kbps*/
            audioEncCtx->bit_rate = outAudioCodecPar->bit_rate;

            const void *sample_fmts;
            int num_sample_fmts;
            int ret_config = avcodec_get_supported_config(audioEncCtx, audioEnc, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, 
                                                        &sample_fmts, &num_sample_fmts);
            if (ret_config >= 0 && num_sample_fmts > 0) {
                audioEncCtx->sample_fmt = *(const enum AVSampleFormat*)sample_fmts;
            } else {
                audioEncCtx->sample_fmt = AV_SAMPLE_FMT_FLTP; // Default fallback
            }

            audioEncCtx->time_base = AVRational{1, audioEncCtx->sample_rate};

            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                audioEncCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            ret = avcodec_open2(audioEncCtx, audioEnc, NULL);
            if (ret < 0) {
                err = alloc_error("Cannot open audio encoder");
                goto cleanup;
            }


            log_debug("Encoder expects: format=%d, sample_rate=%d, ch_layout=%llu", 
                    audioEncCtx->sample_fmt, audioEncCtx->sample_rate, 
                    (unsigned long long)audioEncCtx->ch_layout.nb_channels);

            ret = avcodec_parameters_from_context(out_audio_stream->codecpar, audioEncCtx);

            if (ret < 0) {
                err = alloc_error("Failed to copy encoder parameters to output stream");
                goto cleanup;
            }
        }

        // create video encoder
        {
            videoEnc = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_H264);

            if (!videoEnc) {
                err = alloc_error("Necessary video encoder not found");
                goto cleanup;
            }

            videoEncCtx = avcodec_alloc_context3(videoEnc);

            if (!videoEncCtx) {
                err = alloc_error("Failed to allocate audio encoder context");
                goto cleanup;
            }

            videoEncCtx->height = videoDecCtx->height;
            videoEncCtx->width  = videoDecCtx->width;
            videoEncCtx->pix_fmt = AV_PIX_FMT_YUV420P;
            videoEncCtx->time_base = ifmt_ctx->streams[inVideoStreamIdx]->time_base;
            videoEncCtx->framerate = av_guess_frame_rate(ifmt_ctx, ifmt_ctx->streams[inVideoStreamIdx], NULL);
            // int framerate = 60;
            // videoEncCtx->framerate = {framerate, 1};
            // videoEncCtx->time_base = {1, framerate};

            // change bitrate.
            // videoEncCtx->bit_rate = 800000;  // 800 kbps (example)
            // videoEncCtx->bit_rate = 1600000;  // 800 kbps (example)
            // videoEncCtx->rc_max_rate = videoEncCtx->bit_rate;
            // videoEncCtx->rc_min_rate = videoEncCtx->bit_rate;
            // videoEncCtx->rc_buffer_size = videoEncCtx->bit_rate;

            // av_opt_set(videoEncCtx->priv_data, "crf", "17", 0);
            ExportOptions* expOpts = &exportState->exportOptions;
            float RF = expOpts->CBRRateFactor;
            int len = snprintf(NULL, 0, "%f", RF);
            char* RFStr = (char*) malloc(len + 1);
            snprintf(RFStr, len + 1, "%f", RF);
            av_opt_set(videoEncCtx->priv_data, "crf", RFStr, 0);
            free(RFStr);

            av_opt_set(videoEncCtx->priv_data, "preset", ENCODER_PRESETS[expOpts->encoderPresetIndex], 0);


            // videoEncCtx->width  = videoDecCtx->width  / 2;
            // videoEncCtx->height = videoDecCtx->height / 2;


            // Open the encoder
            ret = avcodec_open2(videoEncCtx, videoEnc, NULL);
            if (ret < 0) {
                err = alloc_error("Failed to open video encoder");
                goto cleanup;
            }

            // Copy encoder parameters to output stream
            // output video to stream 0
            ret = avcodec_parameters_from_context(ofmt_ctx->streams[0]->codecpar, videoEncCtx);
            if (ret < 0) {
                err = alloc_error("Failed to copy video encoder parameters to output stream");
                goto cleanup;
            }

            // Log what we got
            log_debug("Video encoder initialized: %s, %dx%d, fmt=%d, bitrate=%ld",
                    videoEnc->name,
                    videoEncCtx->width, videoEncCtx->height,
                    videoEncCtx->pix_fmt, videoEncCtx->bit_rate);

        }
    }

    if (exportState->clipIndex == 0) {
        ret = avformat_write_header(ofmt_ctx, NULL);
        if (ret < 0) {
            err = alloc_error("Error occurred when opening output file");
            goto cleanup;
        }
    }

    {

        int64_t seek_target = av_rescale_q(start_TS, AV_TIME_BASE_Q, ifmt_ctx->streams[inVideoStreamIdx]->time_base);
        //ret = av_seek_frame(ifmt_ctx, inVideoStreamIdx, seek_target, AVSEEK_FLAG_BACKWARD);
        // we seek with video index, but it seeks for all other streams too to the closest point
        // should i still be using the AV_SEEK_FLAG_BACKWARD flag now that i am always re-encoding video?
        ret = av_seek_frame(ifmt_ctx, inVideoStreamIdx, seek_target, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            err = alloc_error("Failed to seek forward to cutting start in source file");
            goto cleanup;
        }

        // only necessary after seek if we are encoding video.
        if (videoDecCtx) avcodec_flush_buffers(videoDecCtx);
        for (int i = 0; i < enabledAudioStreamCount; i++) {
            avcodec_flush_buffers(audioDecCtx[i]);
        }

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


        // last_audio_dts = 0;
        
        log_debug("entering main decoding encoding loop");
        // main decoding/encoding loop
        bool isPastEndTSVideo = false;
        bool isPastEndTSAudio = false;
        while (1) {
            ret = av_read_frame(ifmt_ctx, pkt);
            if (ret < 0) // end of file
                break;

            int in_index = pkt->stream_index;
            AVStream *in_stream = ifmt_ctx->streams[in_index];

            if ((*exportState->audioStreamDisabled)[in_index]) {
                av_packet_unref(pkt);
                continue;
            }

            if (isPastEndTSVideo && isPastEndTSAudio) {
                break;
            }



            in_stream  = ifmt_ctx->streams[pkt->stream_index];

            
            if (in_index == inVideoStreamIdx) {

                int ret_send = avcodec_send_packet(videoDecCtx, pkt);
                if (ret_send < 0 && ret_send != AVERROR(EAGAIN)) {
                    err = alloc_error("Error sending video packet to decoder");
                    goto cleanup;
                }

                err = recieveAndProcessFramesVideo(
                    exportState,
                    ifmt_ctx,
                    ofmt_ctx,
                    frame,
                    start_TS,
                    end_TS,
                    videoDecCtx,
                    videoEncCtx,
                    in_index,
                    &isPastEndTSVideo,
                    false,
                    found_rebase_pts,
                    pts_offset
                );

                if (err) {
                    av_frame_free(&frame);
                    goto cleanup;
                }


            } else { // if stream is audio
                pkt->stream_index = outAudioStreamIdx;

                int idx = -1;

                for (int i=0; i < enabledAudioStreamCount; i++) {
                    if (in_index == enabledAudioStreamIdx[i]) {
                        idx = i;
                        break;
                    }
                }

                ret = avcodec_send_packet(audioDecCtx[idx], pkt);
                if (ret < 0) {
                    err = alloc_error("Failed to send packet to decoder");
                    goto cleanup;
                }


                if (idx != -1) {
                    ret = avcodec_receive_frame(audioDecCtx[idx], frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        if (ret == AVERROR(EAGAIN)) {
                            av_packet_unref(pkt);
                            continue;
                        }
                        break;
                    } else if (ret < 0) {
                        err = alloc_error("error during decoding audio");
                        goto cleanup;
                    }


                    // It is unclear to me if the line below is necessary
                    // or if the decoder always uses the same time base as the input.
                    // works with our without on mp4s, but hard to test with every codec etc
                    frame->pts = av_rescale_q(frame->pts,
                                            audioDecCtx[idx]->time_base,
                                            in_stream->time_base);
                    int64_t pts_us_frame = av_rescale_q(frame->pts,
                                                        in_stream->time_base,
                                                        AV_TIME_BASE_Q);


                    // drop packet if it's before our span
                    if (pts_us_frame < start_TS) {
                        av_frame_unref(frame);
                        continue;
                    }

                    // If after trimming region → mark this stream as finished
                    if (pts_us_frame > end_TS) {
                        isPastEndTSAudio = true;
                        av_frame_unref(frame);
                        continue;
                    }

                    // Calculate pts_offset if needed
                    if (!found_rebase_pts[in_index]) {
                        pts_offset[in_index] = av_rescale_q(frame->pts,
                                audioDecCtx[idx]->time_base,
                                audioEncCtx->time_base);
                        found_rebase_pts[in_index] = true;
                    }


                    {
                        // progressbar logic
                        double duration = (double) (ifmt_ctx->duration) / AV_TIME_BASE ; // duration in seconds
                        duration = duration - mediaClip->startCutoff - mediaClip->endCutoff;
                        double currentTime = (double) (pts_us_frame / AV_TIME_BASE);
                        currentTime = currentTime-mediaClip->startCutoff;
                        exportState->exportProgress = (float) (currentTime / duration);
                    }



                    // push to buffer source so that it gets processed by our filters
                    ret = av_buffersrc_add_frame_flags(abufferCtxs[idx], frame, AV_BUFFERSRC_FLAG_KEEP_REF);
                    if (ret < 0) {
                        err = alloc_error("Error feeding audio frame to filtergraph");
                        goto cleanup;
                    }

                    // grab from filtergraph
                    while (1) {
                        ret = av_buffersink_get_frame(abuffersink_ctx, frame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        } else if (ret < 0) {
                            err = alloc_error("error getting audio from buffersink");
                            goto cleanup;
                        }

                        // pts is in in_stream->time_base since the abuffer is configured to use that
                        frame->pts = av_rescale_q(frame->pts, in_stream->time_base, audioEncCtx->time_base);

                        // rebase timestamp
                        int64_t rebased_pts = frame->pts - pts_offset[in_index] + exportState->offsetPtsEncTBAudio;

                        // ensure monotically increasing pts
                        int64_t last_pts = *last_audio_pts_enc_tb;
                        if (last_pts != AV_NOPTS_VALUE && rebased_pts <= last_pts) {
                            log_debug("non monotically increasing pts for AUDIO, fixing")
                            rebased_pts = last_pts + 1;
                        }
                        *last_audio_pts_enc_tb = rebased_pts;

                        frame->pts = rebased_pts;

                        ret = avcodec_send_frame(audioEncCtx, frame);
                        if (ret < 0) {
                            err = alloc_error("error sending frame to encoder");
                            goto cleanup;
                        }

                        while (ret >= 0) {
                            ret = avcodec_receive_packet(audioEncCtx, pkt);
                            pkt->stream_index = outAudioStreamIdx; // as calling avcodec_receive_packet resets it
                            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                                break;
                            } else if (ret < 0) {
                                err = alloc_error("error decoding audio frame");
                                goto cleanup;
                            }

                            ret = av_interleaved_write_frame(ofmt_ctx, pkt);
                            if (ret < 0) {
                                err = alloc_error("Error writing audio frame");
                                goto cleanup;
                            }
                        }

                        av_frame_unref(frame);
                    }
                }

            }

            av_packet_unref(pkt);
        }

        // after we are done encoding clip, flush encoders so that we may re-use them for the next clip.
        
        // https://www.ffmpeg.org/doxygen/trunk/group__lavc__encdec.html
        log_debug("draining decoder");
        exportState->statusString = (char*) "Draining decoder/encoder";

        avcodec_send_packet(videoDecCtx, NULL); // enters "draining mode"
        err = recieveAndProcessFramesVideo(
            exportState,
            ifmt_ctx,
            ofmt_ctx,
            frame,
            start_TS,
            end_TS,
            videoDecCtx,
            videoEncCtx,
            pkt->stream_index,
            &isPastEndTSVideo,
            false,
            found_rebase_pts,
            pts_offset
        );

        if (err) {
            av_frame_free(&frame);
            goto cleanup;
        }

        log_debug("draining encoder");
        avcodec_send_frame(videoEncCtx, NULL);

        AVPacket* outpkt = av_packet_alloc();
        while (1) {
            int ret = avcodec_receive_packet(videoEncCtx, outpkt);
            if (ret == AVERROR(EAGAIN)) continue;
            if (ret == AVERROR_EOF) break;

            av_packet_rescale_ts(outpkt, videoEncCtx->time_base, exportState->out_video_stream->time_base);
            outpkt->stream_index = exportState->out_video_stream->index;
            av_interleaved_write_frame(ofmt_ctx, outpkt);
            av_packet_unref(outpkt);
        }

        av_packet_free(&outpkt);


        log_debug("finished draining");

        // Similar flushing for audio encoder


    }

    // av_write_trailer(ofmt_ctx);

cleanup:
    if (pkt)
        av_packet_free(&pkt);

    if (frame)
        av_frame_free(&frame);

    avformat_close_input(&ifmt_ctx);

    avfilter_graph_free(&filter_graph);

    // /* close output */
    // if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
    //     avio_closep(&ofmt_ctx->pb);
    // avformat_free_context(ofmt_ctx);

    // TODO: ability to get multiple error messages
    if (err == nullptr && ret < 0 && ret != AVERROR_EOF) {
        char* errbuf = (char*) malloc(MAX_ERROR_LENGTH);
        av_strerror(ret, errbuf, MAX_ERROR_LENGTH);
        err = errbuf;
    }

    log_debug("finished remuxing with errored as: %d", err);

    return err;
}



int remux_keepMultipleAudioTracks(MediaClip* mediaClip, const char* out_filename);

void exportVideo(App* app, bool combineAudioStreams) {
    /*cc_unused(app);*/

    MediaClip* firstClip = app->mediaClips[0];
    if (!firstClip) {
        char errMsg[] = "There is nothing to export";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Exporting Failed", errMsg, app->window);
        return;
    }

    if (combineAudioStreams) {
        /*char* errMsg = remux(firstClip, &app->exportFrame, app->exportPath);*/
        char* errMsg = remuxMultipleClips(app->mediaClips, &app->exportState, app->exportPath);
        if (errMsg != nullptr) {
            log_info("Exporting failed with error: %s", errMsg);
            app->exportState.statusString = (char*) "Failed";
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Exporting Failed", errMsg, app->window);
        } else {
            app->exportState.statusString = (char*) "Completed";
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Export", "Sucessfully exported video", app->window);
        }
        free(errMsg);
    } else {
        /*int err = remux_keepMultipleAudioTracks(firstClip, "D:/notCDrive/Videos/cc_debug/ffmpeg/cc_output.mp4");*/
        // int err = remux_keepMultipleAudioTracks(firstClip,app->exportPath);
        // log_debug("err: %d", err);
        // if (err) {
        //     SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Exporting Failed", "check logs for error", app->window);
        // } else {
        //     SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Export", "Sucessfully exported video", app->window);
        // }
    }
}
