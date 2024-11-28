
#include <iostream>
#include<vector>
#include<fstream>
#include <iomanip> // For std::setw and std::setfill

extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
#include <stdint.h>
#include <libavutil\pixfmt.h>
#include <libavformat\avformat.h>
#include <libavutil\rational.h>
#include <libswscale\swscale.h>
#include <libavutil\opt.h>
#include <libavutil\error.h>
}
#include <winsock2.h>
#include <ws2tcpip.h>
#include <CVideoStreamEncoder.hpp>

std::string avErrorToString(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, sizeof(errbuf));
    return std::string(errbuf);
}



VideoStreamEncoder::VideoStreamEncoder() {}

VideoStreamEncoder::~VideoStreamEncoder() {
    Close();
}

bool VideoStreamEncoder::Open(const Params& params) {
    Close();

    do {
        avformat_alloc_output_context2(&mContext.format_context, nullptr, "mp4", nullptr);
        if (!mContext.format_context) {
            std::cout << "could not allocate output format" << std::endl;
            break;
        }

        mContext.codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!mContext.codec) {
            std::cout << "could not find encoder" << std::endl;
            break;
        }

        mContext.stream = avformat_new_stream(mContext.format_context, nullptr);
        if (!mContext.stream) {
            std::cout << "could not create stream" << std::endl;
            break;
        }
        mContext.stream->id = (int)(mContext.format_context->nb_streams - 1);

        mContext.codec_context = avcodec_alloc_context3(mContext.codec);
        if (!mContext.codec_context) {
            std::cout << "could not allocate mContext codec context" << std::endl;
            break;
        }

        mContext.codec_context->codec_id = mContext.format_context->oformat->video_codec;
        mContext.codec_context->bit_rate = params.bitrate;
        mContext.codec_context->width = static_cast<int>(params.width);
        mContext.codec_context->height = static_cast<int>(params.height);
        mContext.stream->time_base = av_d2q(1.0 / params.fps, 120);
        mContext.codec_context->time_base = mContext.stream->time_base;
        mContext.codec_context->pix_fmt = params.dst_format;
        mContext.codec_context->gop_size = 12;
        mContext.codec_context->max_b_frames = 2;

        if (mContext.format_context->oformat->flags & AVFMT_GLOBALHEADER)
            mContext.codec_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        int ret = 0;
        if (params.preset) {
            ret = av_opt_set(mContext.codec_context->priv_data, "preset", params.preset, 0);
            if (ret != 0) {
                std::cout << "could not set preset: " << params.preset << std::endl;
                break;
            }
        }

        ret = av_opt_set_int(mContext.codec_context->priv_data, "crf", params.crf, 0);
        if (ret != 0) {
            std::cout << "could not set crf: " << params.crf << std::endl;
            break;
        }

        ret = avcodec_open2(mContext.codec_context, mContext.codec, nullptr);
        if (ret != 0) {
            std::cout << "could not open codec: " << ret << std::endl;
            break;
        }

        mContext.frame = av_frame_alloc();
        if (!mContext.frame) {
            std::cout << "could not allocate mContext frame" << std::endl;
            break;
        }
        mContext.frame->format = mContext.codec_context->pix_fmt;
        mContext.frame->width = mContext.codec_context->width;
        mContext.frame->height = mContext.codec_context->height;

        ret = av_frame_get_buffer(mContext.frame, 32);
        if (ret < 0) {
            std::cout << "could not allocate the mContext frame data" << std::endl;
            break;
        }

        ret = avcodec_parameters_from_context(mContext.stream->codecpar, mContext.codec_context);
        if (ret < 0) {
            std::cout << "could not copy the stream parameters" << std::endl;
            break;
        }

        mContext.sws_context = sws_getContext(
            mContext.codec_context->width, mContext.codec_context->height, params.src_format,   // src
            mContext.codec_context->width, mContext.codec_context->height, params.dst_format, // dst
            SWS_BICUBIC, nullptr, nullptr, nullptr
        );
        if (!mContext.sws_context) {
            std::cout << "could not initialize the conversion context" << std::endl;
            break;
        }

        av_dump_format(mContext.format_context, 0, nullptr, 1);
        ret = avio_open_dyn_buf(&mContext.format_context->pb);
        if (ret < 0) {
            std::cout << "could not open dynamic buffer" << std::endl;
            break;
        }

        ret = avformat_write_header(mContext.format_context, nullptr);
        if (ret < 0) {
            std::cout << "could not write header" << std::endl;
            break;
        }

        mContext.frame_index = 0;
        mIsOpen = true;
        return true;
    } while (false);

    Close();

    return false;
}

void VideoStreamEncoder::Close() {
    if (mIsOpen) {
        avcodec_send_frame(mContext.codec_context, nullptr);
        FlushPackets();
        av_write_trailer(mContext.format_context);

        uint8_t *buffer = nullptr;
        int buffer_size = avio_close_dyn_buf(mContext.format_context->pb, &buffer);
        if (buffer_size >= 0) {
            std::vector<uint8_t> initSegment(buffer, buffer + buffer_size);
            encodedFramesQueue.push(initSegment);
            av_free(buffer);

        }

        if (mContext.sws_context)
            sws_freeContext(mContext.sws_context);

        if (mContext.frame)
            av_frame_free(&mContext.frame);

        if (mContext.codec_context)
            avcodec_free_context(&mContext.codec_context);

        if (mContext.format_context)
            avformat_free_context(mContext.format_context);

        mContext = {};
        mIsOpen = false;

    }


}

bool VideoStreamEncoder::Write(const unsigned char *data) {
    if (!mIsOpen)
        return false;

    auto ret = av_frame_make_writable(mContext.frame);
    if (ret < 0) {
        std::cout << "frame not writable" << std::endl;
        return false;
    }

    const int in_linesize[1] = { mContext.codec_context->width * 3 };

    sws_scale(
        mContext.sws_context,
        &data, in_linesize, 0, mContext.codec_context->height,  // src
        mContext.frame->data, mContext.frame->linesize // dst
    );
    mContext.frame->pts = mContext.frame_index++;

    ret = avcodec_send_frame(mContext.codec_context, mContext.frame);
    if (ret < 0) {
        std::cout << "error sending a frame for encoding" << std::endl;
        return false;
    }

    return FlushPackets();
}
bool VideoStreamEncoder::FlushPackets() {
    int ret;
    do {
        AVPacket packet = { 0 };

        ret = avcodec_receive_packet(mContext.codec_context, &packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;

        if (ret < 0) {
            std::cout << "error encoding a frame: " << ret << std::endl;
            return false;
        }
        // Calculate PTS and DTS
        packet.pts = packet.dts = mContext.frame_index * av_rescale_q(1, mContext.codec_context->time_base, mContext.stream->time_base);
        packet.duration = av_rescale_q(1, mContext.codec_context->time_base, mContext.stream->time_base);
        av_packet_rescale_ts(&packet, mContext.codec_context->time_base, mContext.stream->time_base);
        packet.stream_index = mContext.stream->index;

        // Ensure PTS and DTS are set
        if (packet.pts == AV_NOPTS_VALUE || packet.dts == AV_NOPTS_VALUE) {
            std::cerr << "Error: PTS or DTS is not set" << std::endl;
            av_packet_unref(&packet);
            return false;
        }

        if (!remuxVideo(packet.data, packet.size)) {
            std::cout << "error remuxing packet" << std::endl;
            av_packet_unref(&packet);
            return false;
        }

        av_packet_unref(&packet);
    } while (ret >= 0);

    return true;
}
bool VideoStreamEncoder::remuxVideo(uint8_t* data, int size) {
    AVFormatContext* outputFormatContext = nullptr;
    AVIOContext* outputIOContext = nullptr;
    AVPacket packet;

    // Initialize output format context
    avformat_alloc_output_context2(&outputFormatContext, nullptr, "mp4", nullptr);
    if (!outputFormatContext) {
        std::cerr << "Could not allocate output format context" << std::endl;
        return false;
    }

    avio_open_dyn_buf(&outputIOContext);
    outputFormatContext->pb = outputIOContext;

    // Create a new stream for the output format context
    AVStream* outStream = avformat_new_stream(outputFormatContext, nullptr);
    if (!outStream) {
        std::cerr << "Could not create new stream" << std::endl;
        avformat_free_context(outputFormatContext);
        return false;
    }

    // Copy codec parameters from the input packet to the output stream
    AVCodecParameters* codecpar = avcodec_parameters_alloc();
    codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    codecpar->codec_id = AV_CODEC_ID_H264;
    codecpar->width = mContext.codec_context->width;
    codecpar->height = mContext.codec_context->height;
    codecpar->format = mContext.codec_context->pix_fmt;
    codecpar->bit_rate = mContext.codec_context->bit_rate;
    avcodec_parameters_copy(outStream->codecpar, codecpar);
    avcodec_parameters_free(&codecpar);

    // Set movflags for fragmented MP4
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);

    if (avformat_write_header(outputFormatContext, &opts) < 0) {
        std::cerr << "Error writing header" << std::endl;
        av_dict_free(&opts);
        avformat_free_context(outputFormatContext);
        return false;
    }
    av_dict_free(&opts);

    // Initialize the packet with the provided data
    av_init_packet(&packet);
    packet.data = data;
    packet.size = size;
    packet.stream_index = outStream->index;

    // Set the timestamps for the packet
         // Calculate PTS and DTS
        packet.pts = packet.dts = mContext.frame_index * av_rescale_q(1, mContext.codec_context->time_base, mContext.stream->time_base);
        packet.duration = av_rescale_q(1, mContext.codec_context->time_base, mContext.stream->time_base);
        av_packet_rescale_ts(&packet, mContext.codec_context->time_base, mContext.stream->time_base);
        packet.stream_index = mContext.stream->index;
    // Ensure PTS and DTS are set
    if (packet.pts == AV_NOPTS_VALUE || packet.dts == AV_NOPTS_VALUE) {
        std::cerr << "Error: PTS or DTS is not set" << std::endl;
        avformat_free_context(outputFormatContext);
        return false;
    }

    // Write the packet to the output format context
    if (av_interleaved_write_frame(outputFormatContext, &packet) < 0) {
        std::cerr << "Error while writing output packet" << std::endl;
        avformat_free_context(outputFormatContext);
        return false;
    }

    av_write_trailer(outputFormatContext);

    uint8_t* buffer = nullptr;
    int bufferSize = avio_close_dyn_buf(outputIOContext, &buffer);
    std::vector<uint8_t> fmp4Data(buffer, buffer + bufferSize);
    av_free(buffer);

    encodedFramesQueue.push(fmp4Data);

    avformat_free_context(outputFormatContext);
    return true;
}

bool VideoStreamEncoder::getEncodedFrame(std::vector<uint8_t>& frame) {
    return encodedFramesQueue.pop(frame);
}

bool VideoStreamEncoder::isencodedFramesQueueEmpty() {
    return encodedFramesQueue.empty();
}