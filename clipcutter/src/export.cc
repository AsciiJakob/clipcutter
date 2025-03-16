#include "pch.h"
#include "app.h"
#include <SDL3/SDL_messagebox.h>

#define MAX_SUPPORTED_AUDIO_TRACKS 50
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

// returns pointer to error message or nullptr if successful
char* remux(MediaClip* mediaClip, const char* out_filename) {
    const char* in_filename = mediaClip->source->path;
    const AVOutputFormat *ofmt = NULL;
    AVPacket* pkt = NULL;
    AVFrame* frame = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVFilterContext* abufferCtxs[MAX_SUPPORTED_AUDIO_TRACKS] = { NULL};
    AVFilterContext* abuffersink_ctx;
    AVCodecContext* audioDecCtx[MAX_SUPPORTED_AUDIO_TRACKS];
    AVCodecContext *enc_ctx;
    AVStream* out_audio_stream;
    AVCodec* enc;
    memset(audioDecCtx, NULL, sizeof(audioDecCtx));
    int inVideoStreamIdx = -1;
    int outAudioStreamIdx = -1;
    int audioStreamIdx[MAX_SUPPORTED_AUDIO_TRACKS];
    memset(audioStreamIdx, -1, sizeof(audioStreamIdx));
    int audioStreamCount = 0;
    int streamRescaledStartSeconds[MAX_SUPPORTED_AUDIO_TRACKS+1];
    int streamRescaledEndSeconds[MAX_SUPPORTED_AUDIO_TRACKS+1];
    int ret;
    char* err = nullptr;
    int64_t last_audio_dts;

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

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        err = alloc_error("Could not create output context");
        goto cleanup;
    }

    ofmt = ofmt_ctx->oformat;

    if (true) {
        bool foundVideo = false;
        log_debug("modified nb sttream is: %d", ifmt_ctx->nb_streams);
        for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
            AVStream *inStream = ifmt_ctx->streams[i];
            AVCodecParameters *in_codecpar = inStream->codecpar;

            streamRescaledStartSeconds[i] = av_rescale_q(mediaClip->drawStartCutoff * AV_TIME_BASE, AV_TIME_BASE_Q, inStream->time_base);
            streamRescaledEndSeconds[i] = av_rescale_q((mediaClip->source->length-mediaClip->drawEndCutoff) * AV_TIME_BASE, AV_TIME_BASE_Q, inStream->time_base);

            if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
                in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO) {
                continue;
            }

            if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                log_debug("found video");
                if (foundVideo) {
                    err = alloc_error("Found more than two video streams");
                    goto cleanup;
                }
                foundVideo = true;

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
        if (!foundVideo) {
            err = alloc_error("Found no video source");
            goto cleanup;
        }

        AVStream* out_video_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_video_stream) {
            err = alloc_error("Failed allocating video output stream");
            goto cleanup;
        }
        ret = avcodec_parameters_copy(out_video_stream->codecpar, ifmt_ctx->streams[inVideoStreamIdx]->codecpar);
        if (ret < 0) {
            err = alloc_error("Failed to copy video codec parameters");
            goto cleanup;
        }

        out_audio_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_audio_stream) {
            err = alloc_error("Failed allocating audio output stream");
            goto cleanup;
        }

        if (ret < 0) {
            err = alloc_error("Failed to copy video codec parameters");
            goto cleanup;
        }

        AVCodecParameters* inAudioCodecPar = ifmt_ctx->streams[audioStreamIdx[0]]->codecpar;
        ret = avcodec_parameters_copy(out_audio_stream->codecpar, inAudioCodecPar);
        /*out_audio_stream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;*/
        /*out_audio_stream->codecpar->codec_id = inCodecPar->codec_id;*/
        /*out_audio_stream->codecpar->sample_rate = inCodecPar->sample_rate;*/
        /*out_audio_stream->codecpar->ch_layout.nb_channels = inCodecPar->ch_layout.nb_channels;  // stereo output*/
        /*out_audio_stream->codecpar->bit_rate = inCodecPar->bit_rate;  // 320kbps */

        outAudioStreamIdx = out_audio_stream->index;

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
                    err = alloc_error("Failed to allocate decoder context");
                    goto cleanup;
                }

                ret = avcodec_parameters_to_context(audioDecCtx[i], stream->codecpar);
                if (ret < 0) {
                    err = alloc_error("Failed to copy codec parameters to decoder");
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
            
                /* Set the filter options through the AVOptions API. */
                /*#define INPUT_SAMPLERATE 48000*/
                /*#define INPUT_FORMAT         AV_SAMPLE_FMT_FLTP*/
                /*#define INPUT_CHANNEL_LAYOUT (AVChannelLayout)AV_CHANNEL_LAYOUT_5POINT0*/
                char ch_layout[64];
                av_channel_layout_describe((const AVChannelLayout*)&stream->codecpar->ch_layout, ch_layout, sizeof(ch_layout));
                av_opt_set    (abuffer_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
                av_opt_set    (abuffer_ctx, "sample_fmt", av_get_sample_fmt_name((AVSampleFormat)stream->codecpar->format), AV_OPT_SEARCH_CHILDREN);
                av_opt_set_q  (abuffer_ctx, "time_base", stream->time_base, AV_OPT_SEARCH_CHILDREN);
                av_opt_set_int(abuffer_ctx, "sample_rate", stream->codecpar->sample_rate, AV_OPT_SEARCH_CHILDREN);


                /*char ch_layout[64];*/
                /*AVChannelLayout test = AV_CHANNEL_LAYOUT_5POINT0;*/
                /*av_channel_layout_describe(&test, ch_layout, sizeof(ch_layout));*/
                /*av_opt_set    (abuffer_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);*/
                /*av_opt_set    (abuffer_ctx, "sample_fmt", av_get_sample_fmt_name(INPUT_FORMAT), AV_OPT_SEARCH_CHILDREN);*/
                /*av_opt_set_q  (abuffer_ctx, "time_base", AVRational{ 1, INPUT_SAMPLERATE }, AV_OPT_SEARCH_CHILDREN);*/
                /*av_opt_set_int(abuffer_ctx, "sample_rate", INPUT_SAMPLERATE, AV_OPT_SEARCH_CHILDREN);*/

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

        /* A third way of passing the options is in a string of the form
        * key1=value1:key2=value2.... */
        char options_str[1024];
        snprintf(options_str, sizeof(options_str),
                "sample_fmts=%s:sample_rates=%d:channel_layouts=stereo",
                av_get_sample_fmt_name((AVSampleFormat)inAudioCodecPar->format), inAudioCodecPar->sample_rate);
        ret = avfilter_init_str(aformat_ctx, options_str);
        if (ret < 0) {
            err = alloc_error("Could not initialize the aformat filter");
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

        /* This filter takes no options. */
        ret = avfilter_init_str(abuffersink_ctx, NULL);
        if (ret < 0) {
            err = alloc_error("Could not initialize the abuffersink instance.");
            goto cleanup;
        }

        for (int i=0; i < audioStreamCount; i++) {
            if (ret >= 0)
                ret = avfilter_link(abufferCtxs[i], 0, amix_ctx, i);
        }
        if (ret >= 0)
            ret = avfilter_link(amix_ctx, 0, aformat_ctx, 0);

        if (ret >= 0)
            ret = avfilter_link(aformat_ctx, 0, abuffersink_ctx, 0);
        if (ret < 0) {
            err = alloc_error("Error connecting filters");
            goto cleanup;
        }


        ret = avfilter_graph_config(filter_graph, NULL);
        if (ret < 0) {
            err = alloc_error("Error configuring the filter graph");
            goto cleanup;
        }

        enc = (AVCodec*) avcodec_find_encoder(ofmt->audio_codec);
        enc_ctx = avcodec_alloc_context3(enc);
        
        if (!enc) {
            err = alloc_error("Necessary encoder not found");
            goto cleanup;
        }

        if (!enc_ctx) {
            err = alloc_error("Failed to allocate encoder context");
            goto cleanup;
        }

        enc_ctx->sample_rate = audioDecCtx[0]->sample_rate;
        /*av_channel_layout_default(&enc_ctx->ch_layout, 2); // stereo layout*/
        av_channel_layout_default(&enc_ctx->ch_layout, inAudioCodecPar->ch_layout.nb_channels); // stereo layout
        /*enc_ctx->bit_rate = 320000;  // 320kbps*/
        enc_ctx->bit_rate = inAudioCodecPar->bit_rate;

        const void *sample_fmts;
        int num_sample_fmts;
        int ret_config = avcodec_get_supported_config(enc_ctx, enc, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, 
                                                    &sample_fmts, &num_sample_fmts);
        if (ret_config >= 0 && num_sample_fmts > 0) {
            enc_ctx->sample_fmt = *(const enum AVSampleFormat*)sample_fmts;
        } else {
            enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP; // Default fallback
        }

        enc_ctx->time_base = AVRational{1, enc_ctx->sample_rate};

        if (ofmt->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        ret = avcodec_open2(enc_ctx, enc, NULL);
        if (ret < 0) {
            err = alloc_error("Cannot open audio encoder");
            goto cleanup;
        }


        log_debug("Encoder expects: format=%d, sample_rate=%d, ch_layout=%llu", 
                enc_ctx->sample_fmt, enc_ctx->sample_rate, 
                (unsigned long long)enc_ctx->ch_layout.nb_channels);

        ret = avcodec_parameters_from_context(out_audio_stream->codecpar, enc_ctx);

        if (ret < 0) {
            err = alloc_error("Failed to copy encoder parameters to output stream");
            goto cleanup;
        }

        ret = avformat_seek_file(ifmt_ctx, -1, INT64_MIN, mediaClip->drawStartCutoff * AV_TIME_BASE, mediaClip->drawStartCutoff * AV_TIME_BASE, 0);
        if (ret < 0) {
            err = alloc_error("Failed to seek forward to cutting start in source file");
            goto cleanup;
        }

        // only necessary after seek if we are encoding video.
        /*avcodec_flush_buffers( inVideoStream->codec );*/


        av_dump_format(ofmt_ctx, 0, out_filename, 1);

        // open output file for writing
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
            if (ret < 0) {
                err = alloc_error("Could not open output file '%s'", out_filename);
                goto cleanup;
            }
        }

        ret = avformat_write_header(ofmt_ctx, NULL);
        if (ret < 0) {
            err = alloc_error("Error occurred when opening output file");
            goto cleanup;
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
    }

        

    last_audio_dts = 0;
    // main decoding/encoding loop
    while (1) {
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret < 0)
            break;

        if (pkt->pts > streamRescaledEndSeconds[pkt->stream_index]) {
            av_packet_unref(pkt);
            continue;
        }


        int in_index = pkt->stream_index;
        in_stream  = ifmt_ctx->streams[pkt->stream_index];

        
        if (in_index == inVideoStreamIdx) {
            pkt->stream_index = 0; // write video output to first stream
            out_stream = ofmt_ctx->streams[pkt->stream_index];

            // shift the packet to its new position by subtracting the rescaled start seconds.
            pkt->pts -= streamRescaledStartSeconds[in_index];
            pkt->dts -= streamRescaledStartSeconds[in_index];

            /* copy packet */
            av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
            pkt->pos = -1;

            ret = av_interleaved_write_frame(ofmt_ctx, pkt);
            if (ret < 0) {
                err = alloc_error("Error writing video frame");
                break;
            }

        } else { // if stream is audio
            pkt->stream_index = outAudioStreamIdx;
            out_stream = ofmt_ctx->streams[pkt->stream_index];

            // shift the packet to its new position by subtracting the rescaled start seconds.
            pkt->pts -= streamRescaledStartSeconds[in_index];
            pkt->dts -= streamRescaledStartSeconds[in_index];

            pkt->pos = -1;

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
                    ret = avcodec_send_frame(enc_ctx, frame);
                    if (ret < 0) {
                        err = alloc_error("error sending frame to encoder");
                        goto cleanup;
                    }

                    while (ret >= 0) {
                        ret = avcodec_receive_packet(enc_ctx, pkt);
                        pkt->stream_index = outAudioStreamIdx; // as calling avcodec_receive_packet resets it
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        } else if (ret < 0) {
                            err = alloc_error("error decoding audio frame");
                            goto cleanup;
                        }

                        av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);

                        // we have to ensure monotonically increasing DTS
                        // the last_audio_dts > 0 condition is because last_audio_dts is 0 for the very first audio frame
                        if (pkt->dts <= last_audio_dts && last_audio_dts > 0) {
                            pkt->dts = last_audio_dts + 1;
                            // if PTS should be after DTS, adjust it accordingly
                            if (pkt->pts <= pkt->dts) {
                                pkt->pts = pkt->dts;
                            }
                        }
                        
                        last_audio_dts = pkt->dts;
                        cc_unused(last_audio_dts);


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


    av_write_trailer(ofmt_ctx);

cleanup:
    if (pkt)
        av_packet_free(&pkt);

    if (frame)
        av_frame_free(&frame);

    avformat_close_input(&ifmt_ctx);

    avfilter_graph_free(&filter_graph);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

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

bool exportVideo(App* app) {
    /*cc_unused(app);*/
    bool combineAudioStreams = true;

    MediaClip* firstClip = app->mediaClips[1];

    if (combineAudioStreams) {
        char* errMsg = remux(firstClip, "D:/notCDrive/Videos/cc_debug/ffmpeg/cc_output.mp4");
        if (errMsg != nullptr) {
            log_info("Exporting failed with error: %s", errMsg);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Exporting Failed", errMsg, app->window);
        } else {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Export", "Sucessfully exported video", app->window);
        }
        free(errMsg);
    } else {
        int err = remux_keepMultipleAudioTracks(firstClip, "D:/notCDrive/Videos/cc_debug/ffmpeg/cc_output.mp4");
        log_debug("err: %d", err);
        if (err) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Exporting Failed", "check logs for error", app->window);
        } else {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Export", "Sucessfully exported video", app->window);
        }
    }

    return true;
}



int remux_keepMultipleAudioTracks(MediaClip* mediaClip, const char* out_filename) {
    const char* in_filename = mediaClip->source->path;
    const AVOutputFormat *ofmt = NULL;
    AVPacket* pkt = NULL;
    AVFrame* frame = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVCodecContext* audioDecCtx[MAX_SUPPORTED_AUDIO_TRACKS];
    memset(audioDecCtx, NULL, sizeof(audioDecCtx));
    int audioStreamIdx[MAX_SUPPORTED_AUDIO_TRACKS];
    memset(audioStreamIdx, -1, sizeof(audioStreamIdx));
    int streamRescaledStartSeconds[MAX_SUPPORTED_AUDIO_TRACKS+1];
    int streamRescaledEndSeconds[MAX_SUPPORTED_AUDIO_TRACKS+1];
    int ret;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
    bool errored = false;
    int stream_index = 0;

    AVFilterGraph* filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        log_error("Unable to create filter graph");
        errored = true;
        return false;
    }

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        log_error("Could not open input file '%s'", in_filename);
        errored = true;
        goto end;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        log_error("Failed to retrieve input stream information");
        errored = true;
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        log_error("Could not create output context");
        ret = AVERROR_UNKNOWN;
        errored = true;
        goto end;
    }

    stream_mapping_size = ifmt_ctx->nb_streams;
    stream_mapping = (int*) av_calloc(stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        errored = true;
        goto end;
    }

    ofmt = ofmt_ctx->oformat;

    log_debug("default nb sttream is: %d", ifmt_ctx->nb_streams);
    for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *inStream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = inStream->codecpar;

        streamRescaledStartSeconds[i] = av_rescale_q(mediaClip->drawStartCutoff * AV_TIME_BASE, AV_TIME_BASE_Q, inStream->time_base);
        streamRescaledEndSeconds[i] = av_rescale_q((mediaClip->source->length-mediaClip->drawEndCutoff) * AV_TIME_BASE, AV_TIME_BASE_Q, inStream->time_base);

        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }

        stream_mapping[i] = stream_index++;

        AVStream* out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            log_error("Failed allocating output stream");
            ret = AVERROR_UNKNOWN;
            errored = true;
            goto end;
        }

        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            log_error("Failed to copy codec parameters");
            errored = true;
            goto end;
        }
        out_stream->codecpar->codec_tag = 0;
    }


    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    // open output file for writing
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            log_error("Could not open output file '%s'", out_filename);
            errored = true;
            goto end;
        }
    }

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        log_error("Error occurred when opening output file");
        errored = true;
        goto end;
    }


    pkt = av_packet_alloc();
    if (!pkt) {
        log_error("Could not allocate AVPacket");
        errored = true;
        goto end;
    }
    frame = av_frame_alloc();
    if (!frame) {
        log_error("Failed to allocate AVFrame");
        errored = true;
        goto end;
    }

        
    ret = avformat_seek_file(ifmt_ctx, -1, INT64_MIN, mediaClip->drawStartCutoff * AV_TIME_BASE, mediaClip->drawStartCutoff * AV_TIME_BASE, 0);
    if (ret < 0) {
        log_error("Failed to seek forward to cutting start in source file");
        errored = true;
        goto end;
    }

    // only necessary after seek if we are encoding video.
    /*avcodec_flush_buffers( inVideoStream->codec );*/

    while (1) {
        AVStream *in_stream, *out_stream;

        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret < 0)
            break;


        if (pkt->pts > streamRescaledEndSeconds[pkt->stream_index]) {
            av_packet_unref(pkt);
            continue;
        }

        int in_index = pkt->stream_index;
        in_stream  = ifmt_ctx->streams[pkt->stream_index];
        if (pkt->stream_index >= stream_mapping_size ||
            stream_mapping[pkt->stream_index] < 0) {
            av_packet_unref(pkt);
            continue;
        }

        pkt->stream_index = stream_mapping[pkt->stream_index];
        out_stream = ofmt_ctx->streams[pkt->stream_index];
        /*log_packet(ifmt_ctx, pkt, "in");*/


        pkt->pts -= streamRescaledStartSeconds[in_index];
        pkt->dts -= streamRescaledStartSeconds[in_index];

        /* copy packet */
        av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
        pkt->pos = -1;
        /*log_packet(ofmt_ctx, pkt, "out");*/

        ret = av_interleaved_write_frame(ofmt_ctx, pkt);
        /* pkt is now blank (av_interleaved_write_frame() takes ownership of
        * its contents and resets pkt), so that no unreferencing is necessary.
        * This would be different if one used av_write_frame(). */
        if (ret < 0) {
            log_error("Error muxing packet");
            break;
        }

        av_packet_unref(pkt);
    }

    av_write_trailer(ofmt_ctx);

end:
    if (pkt)
        av_packet_free(&pkt);

    if (frame)
        av_frame_free(&frame);

    avformat_close_input(&ifmt_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    av_freep(&stream_mapping);

    if (ret < 0 && ret != AVERROR_EOF) {
        char errbuf[512];
        av_strerror(ret, errbuf, sizeof(errbuf));
        log_error("Error occured: %s", errbuf);

        /*fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));*/
        errored = true;
    }

    log_debug("finished remuxing with errored as: %d", errored);

    return errored;

}
