#include "pch.h"
#include "app.h"
#include "mediaSource.h"
#include <SDL3/SDL_messagebox.h>

#define MAX_ERROR_LENGTH 4096

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
    app->exportState.exportOptions.exportVideo = true;
    app->exportState.exportOptions.exportAudio = true;
    app->exportState.exportOptions.remuxVideo = true;
    app->exportState.exportOptions.remuxAudio = false;
    app->exportState.exportOptions.mergeAudioTracks = true;
}

void Export_SetDefaultExportOptionsAudio(App* app) {
    app->exportState.exportOptions.exportVideo = false;
    app->exportState.exportOptions.exportAudio = true;
    app->exportState.exportOptions.remuxVideo = false;
    app->exportState.exportOptions.remuxAudio = false;
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
        log_debug("modified nb sttream is: %d", ifmt_ctx->nb_streams);
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

        if (audioStreamIdx[0] == -1) {
            err = alloc_error("found no audio sources");
            goto cleanup;
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
    exportState->offsetPtsEncTB = 0; 
    exportState->audioOffsetPts = 0; 
    exportState->lastVideoPtsEncTB = AV_NOPTS_VALUE;
    exportState->lastDts = AV_NOPTS_VALUE;
    exportState->lastAudioPts = AV_NOPTS_VALUE;
    exportState->lastAudioDts = AV_NOPTS_VALUE;
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
        if (exportState->lastVideoPtsEncTB != AV_NOPTS_VALUE) {
            // +1 since we want to write the frame after the last one.
            exportState->offsetPtsEncTB = exportState->lastVideoPtsEncTB+1; 
            exportState->audioOffsetPts = exportState->lastAudioPts+1; 
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


// void encode_packet(AVCodecContext *dec_ctx,
//                               AVCodecContext *enc_ctx,
//                               AVStream *in_stream,
//                               AVStream *out_stream,
//                               int64_t *pts_offset,
//                               bool *found_rebase_pts,
//                               int64_t *prev_frame_pts,
//                               int64_t start_TS,
//                               int64_t end_TS)
// {
//
//
// }



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
    int audioStreamIdx[MAX_SUPPORTED_AUDIO_TRACKS];
    memset(audioStreamIdx, -1, sizeof(audioStreamIdx));
    int audioStreamCount = 0;
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


    // , int64_t* offsetPts, int64_t* offsetDts, int64_t* lastPts, int64_t* lastDts   
    const char* out_filename = exportState->out_filename;
    log_debug("filename is: %s", out_filename);
    AVStream* out_audio_stream  = exportState->out_audio_stream;
    int64_t* last_video_pts_enc_tb = &exportState->lastVideoPtsEncTB;


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
                log_debug("found audio");
                audioStreamIdx[audioStreamCount++] = i;
            }
        }

        if (audioStreamIdx[0] == -1) {
            err = alloc_error("found no audio sources");
            goto cleanup;
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
            for (int i=0; i < audioStreamCount; i++) {
                const AVStream* stream = ifmt_ctx->streams[audioStreamIdx[i]];
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

        av_opt_set_int(amix_ctx, "inputs", audioStreamCount, AV_OPT_SEARCH_CHILDREN);

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

        // for (int i=0; i < audioStreamCount; i++) {
        //     if (ret >= 0)
        //         ret = avfilter_link(abufferCtxs[i], 0, amix_ctx, i);
        // }
        // if (ret >= 0)
        //     ret = avfilter_link(amix_ctx, 0, aformat_ctx, 0);
        // if (ret >= 0)
        //     ret = avfilter_link(aformat_ctx, 0, abuffersink_ctx, 0);
        // if (ret < 0) {
        //     err = alloc_error("Error connecting filters");
        //     goto cleanup;
        // }
        for (int i=0; i < audioStreamCount; i++) {
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

        // create audio encoder
        {
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
            // videoEncCtx->time_base = vidStream->time_base;
            // videoEncCtx->framerate = av_guess_frame_rate(ifmt_ctx, vidStream, NULL);
            int framerate = 30;
            videoEncCtx->framerate = {framerate, 1};
            videoEncCtx->time_base = {1, framerate};


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
        for (int i = 0; i < audioStreamCount; i++) {
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
        
        // main decoding/encoding loop
        bool finished_video = false;
        bool finished_audio = false;
        while (1) {
            ret = av_read_frame(ifmt_ctx, pkt);
            if (ret < 0) // end of file
                break;

            int in_index = pkt->stream_index;
            AVStream *in_stream = ifmt_ctx->streams[pkt->stream_index];

            // if (is_muted_track[in_index)) {
            //     continue;
            // }

            if (finished_video && finished_audio) {
                break;
            }



            in_stream  = ifmt_ctx->streams[pkt->stream_index];

            
            if (in_index == inVideoStreamIdx) {

                // pkt->pts -= streamRescaledStartSeconds[in_index];
                // pkt->dts -= streamRescaledStartSeconds[in_index];

                int ret_send = avcodec_send_packet(videoDecCtx, pkt);
                if (ret_send < 0 && ret_send != AVERROR(EAGAIN)) {
                    err = alloc_error("Error sending video packet to decoder");
                    goto cleanup;
                }

                while (ret_send >= 0) {
                    ret_send = avcodec_receive_frame(videoDecCtx, frame);
                    if (ret_send == AVERROR(EAGAIN) || ret_send == AVERROR_EOF) {
                        break;
                    }

                    if (ret_send < 0) {
                        err = alloc_error("Error during video decoding");
                        av_frame_free(&frame);
                        goto cleanup;
                    }

                    frame->pts = av_rescale_q(frame->pts, in_stream->time_base, videoEncCtx->time_base);

                    int64_t pts_us_frame = av_rescale_q(frame->pts,
                                                        videoEncCtx->time_base,
                                                        AV_TIME_BASE_Q);

                    // drop packet if it's before our span
                    if (pts_us_frame < start_TS) {
                        av_frame_unref(frame);
                        continue;
                    }

                    // if after trimming region, mark this stream as finished
                    if (pts_us_frame > end_TS) {
                        log_debug("reached ")
                        finished_video = true;
                        av_frame_unref(frame);
                        continue;
                    }
                    log_debug("writing video with pts %d", pts_us_frame)

                    // calculate pts_offset if needed
                    if (!found_rebase_pts[in_index]) {
                        // pts_offset[in_index] = exportState->offsetPts+pts_us_frame;
                        pts_offset[in_index] = frame->pts; // frame->pts is in enc TB
                        found_rebase_pts[in_index] = true;
                    }

                    // rebase timestamp
                    int64_t rebased_pts = frame->pts - pts_offset[in_index];
                    
                    // ensure monotically increasing pts
                    int64_t last_pts = *last_video_pts_enc_tb;
                    if (last_pts != AV_NOPTS_VALUE && rebased_pts <= last_pts) {
                        log_debug("non monotically increasing pts for video, fixing")
                        rebased_pts = last_pts + 1;
                    }
                    *last_video_pts_enc_tb = rebased_pts;

                    frame->pts = rebased_pts;

                    log_debug("output frame_pts in encoder TS: %d", frame->pts);

                    int ret_send_frame = avcodec_send_frame(videoEncCtx, frame);
                    if (ret_send_frame < 0 && ret_send_frame != AVERROR(EAGAIN)) {
                        err = alloc_error("Error sending frame to encoder");
                        goto cleanup;
                    }

                    AVPacket* enc_pkt = av_packet_alloc();

                    while (avcodec_receive_packet(videoEncCtx, enc_pkt) >= 0) {
                        enc_pkt->stream_index = 0;

                        av_packet_rescale_ts(enc_pkt, videoEncCtx->time_base, ofmt_ctx->streams[inVideoStreamIdx]->time_base); 
                        // write encoded video
                        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
                        if (ret < 0) {
                            err = alloc_error("Error writing re-encoded video frame");
                            av_packet_free(&enc_pkt);
                            av_frame_unref(frame);
                            goto cleanup;
                        }
                        
                        av_packet_unref(enc_pkt);

                    }
                    av_packet_free(&enc_pkt);
                    av_frame_unref(frame);
                }

            } else { // if stream is audio
                pkt->stream_index = outAudioStreamIdx;

                int idx = -1;

                for (int i=0; i < audioStreamCount; i++) {
                    if (in_index == audioStreamIdx[i]) {
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
                        finished_audio = true;
                        av_frame_unref(frame);
                        continue;
                    }

                    // Calculate pts_offset if needed
                    if (!found_rebase_pts[in_index]) {
                        // pts_offset[in_index] = exportState->offsetPts+pts_us_frame;
                        pts_offset[in_index] = frame->pts;
                        found_rebase_pts[in_index] = true;
                    }

                    // rebase timestamp
                    int64_t rebased_pts = frame->pts - pts_offset[in_index];

                    frame->pts = av_rescale_q(rebased_pts, in_stream->time_base, audioDecCtx[idx]->time_base);


                    {
                        // progressbar logic
                        double duration = (double) (ifmt_ctx->duration) / AV_TIME_BASE - mediaClip->startCutoff-mediaClip->endCutoff;
                        double offset_from_rebase = pts_us_frame - (pts_offset[in_index]-exportState->offsetPtsEncTB);
                        double currentTime = (double) offset_from_rebase * av_q2d(in_stream->time_base);
                        // if (rebased_pts < offset_from_rebase) {
                        //     currentTime = 0.0;
                        // }

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

                        //log_debug("Sending frame to encoder: format=%d, samples=%d, channels=%d", 
                        //       frame->format, frame->nb_samples, frame->ch_layout.nb_channels);
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

                            // av_packet_rescale_ts(pkt, audioEncCtx->time_base, out_stream->time_base);



                            // we have to ensure monotonically increasing DTS
                            // the last_audio_dts > 0 condition is because last_audio_dts is 0 for the very first audio frame
                            // if (pkt->dts <= *last_audio_dts && *last_audio_dts > 0) {
                            // if (*last_audio_dts != AV_NOPTS_VALUE && pkt->dts <= *last_audio_dts) {
                            // if (exportState->lastAudioDts != AV_NOPTS_VALUE && pkt->dts <= exportState->lastAudioDts) {
                            //     log_debug("we have to ensure it is increasing");
                            //     pkt->dts = exportState->lastAudioDts + 1;
                            //     // if PTS is after DTS, adjust it accordingly
                            //     if (pkt->pts <= pkt->dts) {
                            //         pkt->pts = pkt->dts;
                            //     }
                            // }
                            // 
                            // exportState->lastAudioDts = pkt->dts;
                            // if (pkt->duration > 0)  {
                            //     exportState->lastAudioPts = pkt->pts+pkt->duration;
                            // } else {
                            //     exportState->lastAudioPts = pkt->pts;
                            // }

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
        avcodec_send_packet(videoDecCtx, NULL); // enters "draining mode"
        while (1) {
            int ret = avcodec_receive_frame(videoDecCtx, frame);
            if (ret == AVERROR_EOF) break;
            if (ret == AVERROR(EAGAIN)) continue;
            log_debug("drained packet recieved");
            int in_index = pkt->stream_index;
            AVStream* in_stream = ifmt_ctx->streams[in_index];

            frame->pts = av_rescale_q(frame->pts, in_stream->time_base, videoEncCtx->time_base);

            frame->pts = frame->best_effort_timestamp;
            if (frame->pts == AV_NOPTS_VALUE) {
                frame->pts = *last_video_pts_enc_tb++;
                *last_video_pts_enc_tb = frame->pts;
            }

            int64_t pts_us_frame = av_rescale_q(frame->pts, videoEncCtx->time_base, AV_TIME_BASE_Q);

            if (pts_us_frame >= start_TS && pts_us_frame <= end_TS) {

                log_debug("writing video with pts %d", pts_us_frame)

                if (!found_rebase_pts[in_index]) {
                    pts_offset[in_index] = frame->pts; // frame->pts is in enc TB
                    found_rebase_pts[in_index] = true;
                }

                // rebase timestamp
                int64_t rebased_pts = frame->pts - pts_offset[in_index];

                // ensure monotically increasing pts
                int64_t last_pts = *last_video_pts_enc_tb;
                // TODO: i think the AV_NOPTS_VALUE check here is redundant.
                if (last_pts != AV_NOPTS_VALUE && rebased_pts <= last_pts) {
                    log_debug("non monotically increasing pts for video in draining, fixing")
                    rebased_pts = last_pts + 1;
                }
                *last_video_pts_enc_tb = rebased_pts;

                frame->pts = rebased_pts;

                log_debug("output frame_pts in encoder TS: %d", frame->pts);

                avcodec_send_frame(videoEncCtx, frame);

                AVPacket *enc_pkt = av_packet_alloc();
                while (1) {
                    int er = avcodec_receive_packet(videoEncCtx, enc_pkt);
                    if (er == AVERROR(EAGAIN) || er == AVERROR_EOF) break;

                    av_packet_rescale_ts(enc_pkt, videoEncCtx->time_base, ofmt_ctx->streams[inVideoStreamIdx]->time_base);
                    enc_pkt->stream_index = 0;
                    av_interleaved_write_frame(ofmt_ctx, enc_pkt);
                    av_packet_unref(enc_pkt);
                }
            }

            av_frame_unref(frame);
        }

        log_debug("draining encoder");
        avcodec_send_frame(videoEncCtx, NULL);

        AVPacket* outpkt = av_packet_alloc();
        while (1) {
            int ret = avcodec_receive_packet(videoEncCtx, outpkt);
            if (ret == AVERROR(EAGAIN)) continue;
            if (ret == AVERROR_EOF) break;


            log_debug("wrote packet from encoder.");
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
