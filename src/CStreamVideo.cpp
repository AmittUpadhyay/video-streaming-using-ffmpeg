#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <thread>
#include <conio.h> 
#include <iostream>
#include <string>
#include <atomic> 
#include <algorithm>
#include <CFFmpegEncoder.hpp>
#include <CVideoCaptureGUI.hpp>
#include <CVideoStreamSocket.hpp>
#include <CVideoStreamEncoder.hpp>


extern "C" {
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}


#include<CStreamVideo.hpp>
// Global variables



videoStream::videoStream(): m_formatContext{nullptr}, 
                            m_decoderContext{nullptr}, 
                            m_videoStreamIndex{-1}                                                   
{
    //do nothing
}

videoStream::~videoStream()
{
    cleanupCamera();
}


bool videoStream::remuxVideo(const char* inputFilename, const char* outputFilename) {
    AVFormatContext* inputFormatContext = nullptr;
    AVFormatContext* outputFormatContext = nullptr;
    AVPacket packet;

    if (avformat_open_input(&inputFormatContext, inputFilename, nullptr, nullptr) < 0) {
        std::cerr << "Could not open input file." << std::endl;
        return false;
    }

    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        std::cerr << "Could not find stream information." << std::endl;
        avformat_close_input(&inputFormatContext);
        return false;
    }

    avformat_alloc_output_context2(&outputFormatContext, nullptr, "mp4", outputFilename);
    if (!outputFormatContext) {
        std::cerr << "Could not create output context." << std::endl;
        avformat_close_input(&inputFormatContext);
        return false;
    }

    for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
        AVStream* inStream = inputFormatContext->streams[i];
        AVStream* outStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outStream) {
            std::cerr << "Failed to allocate output stream." << std::endl;
            avformat_close_input(&inputFormatContext);
            avformat_free_context(outputFormatContext);
            return false;
        }

        if (avcodec_parameters_copy(outStream->codecpar, inStream->codecpar) < 0) {
            std::cerr << "Failed to copy codec parameters." << std::endl;
            avformat_close_input(&inputFormatContext);
            avformat_free_context(outputFormatContext);
            return false;
        }
        outStream->codecpar->codec_tag = 0;
    }

    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputFormatContext->pb, outputFilename, AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open output file." << std::endl;
            avformat_close_input(&inputFormatContext);
            avformat_free_context(outputFormatContext);
            return false;
        }
    }

    // Set the movflags to enable fragmentation
    av_dict_set(&outputFormatContext->metadata, "movflags", "frag_keyframe+empty_moov+default_base_moof", 0);

    if (avformat_write_header(outputFormatContext, nullptr) < 0) {
        std::cerr << "Error occurred when opening output file." << std::endl;
        avformat_close_input(&inputFormatContext);
        avformat_free_context(outputFormatContext);
        return false;
    }

    while (av_read_frame(inputFormatContext, &packet) >= 0) {
        AVStream* inStream = inputFormatContext->streams[packet.stream_index];
        AVStream* outStream = outputFormatContext->streams[packet.stream_index];

        packet.pts = av_rescale_q_rnd(packet.pts, inStream->time_base, outStream->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, inStream->time_base, outStream->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, inStream->time_base, outStream->time_base);
        packet.pos = -1;

        if (av_interleaved_write_frame(outputFormatContext, &packet) < 0) {
            std::cerr << "Error muxing packet." << std::endl;
            break;
        }
        av_packet_unref(&packet);
    }

    av_write_trailer(outputFormatContext);
    avformat_close_input(&inputFormatContext);
    if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&outputFormatContext->pb);
    }
    avformat_free_context(outputFormatContext);

    return true;
}

bool videoStream::initializeCamera() {
    avdevice_register_all();
    
    m_formatContext = avformat_alloc_context();
    const AVInputFormat* inputFormat = av_find_input_format("dshow");
    AVDictionary* options = nullptr;
    av_dict_set(&options, "rtbufsize", "100M", 0); // Increase buffer size to 100MB

    if (avformat_open_input(&m_formatContext, "video=Integrated Webcam", inputFormat, &options) != 0) {
        fprintf(stderr, "Could not open video device\n");
        return false;
    }
    av_dict_free(&options);


    if (avformat_find_stream_info(m_formatContext, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        avformat_close_input(&m_formatContext);
        return false;
    }

    const AVCodec* decoder = nullptr;
    AVCodecParameters* codecParams = nullptr;

    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            codecParams = m_formatContext->streams[i]->codecpar;
            decoder = avcodec_find_decoder(codecParams->codec_id);
            break;
        }
    }

    if (m_videoStreamIndex == -1) {
        fprintf(stderr, "Could not find video stream\n");
        avformat_close_input(&m_formatContext);
        return false;
    }

    if (!decoder) {
        fprintf(stderr, "Could not find decoder\n");
        avformat_close_input(&m_formatContext);
        return false;
    }

    m_decoderContext = avcodec_alloc_context3(decoder);
    if (!m_decoderContext) {
        fprintf(stderr, "Could not allocate decoder context\n");
        avformat_close_input(&m_formatContext);
        return false;
    }

    if (avcodec_parameters_to_context(m_decoderContext, codecParams) < 0) {
        fprintf(stderr, "Could not copy codec parameters to decoder context\n");
        avcodec_free_context(&m_decoderContext);
        avformat_close_input(&m_formatContext);
        return false;
    }

    if (avcodec_open2(m_decoderContext, decoder, NULL) < 0) {
        fprintf(stderr, "Could not open decoder\n");
        avcodec_free_context(&m_decoderContext);
        avformat_close_input(&m_formatContext);
        return false;
    }

    return true;
}

unsigned char* videoStream::getFrameData(int& width, int& height) {
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "Could not allocate frame\n";
        return nullptr;
    }
    AVPacket packet;
    av_init_packet(&packet);
    while (m_recording.load()) {
        if (av_read_frame(m_formatContext, &packet) >= 0) {
            if (packet.stream_index == m_videoStreamIndex) {
                if (avcodec_send_packet(m_decoderContext, &packet) == 0) {
                    if (avcodec_receive_frame(m_decoderContext, frame) == 0) {
                        width = frame->width;
                        height = frame->height;
                        // Allocate buffer for RGB data
                        AVFrame* rgbFrame = av_frame_alloc();
                        if (!rgbFrame) {
                            std::cerr << "Could not allocate RGB frame\n";
                            av_packet_unref(&packet);
                            av_frame_free(&frame);
                            return nullptr;
                        }
                        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
                        unsigned char* rgbBuffer = (unsigned char*)av_malloc(numBytes * sizeof(unsigned char));
                        if (!rgbBuffer) {
                            std::cerr << "Could not allocate RGB buffer\n";
                            av_packet_unref(&packet);
                            av_frame_free(&frame);
                            av_frame_free(&rgbFrame);
                            return nullptr;
                        }
                        av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, rgbBuffer, AV_PIX_FMT_RGB24, width, height, 1);
                        // Initialize the conversion context with color range
                        struct SwsContext* swsCtx = sws_getContext(
                            frame->width, frame->height, AV_PIX_FMT_YUV420P, // Source dimensions and format
                            width, height, AV_PIX_FMT_RGB24, // Destination dimensions and format
                            SWS_LANCZOS, nullptr, nullptr, nullptr
                        );
                        if (!swsCtx) {
                            std::cerr << "Could not initialize sws context\n";
                            av_packet_unref(&packet);
                            av_frame_free(&frame);
                            av_frame_free(&rgbFrame);
                            av_free(rgbBuffer);
                            return nullptr;
                        }
                        // Set color range
                        av_opt_set_int(swsCtx, "src_range", 1, 0); // Full range for source
                        av_opt_set_int(swsCtx, "dst_range", 1, 0); // Full range for destination
                        // Convert the frame
                        sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, rgbFrame->data, rgbFrame->linesize);
                        // Clean up
                        sws_freeContext(swsCtx);
                        av_packet_unref(&packet);
                        av_frame_free(&frame);
                        return rgbFrame->data[0];
                    } else {
                        std::cerr << "Error receiving frame, skipping corrupted frame.\n";
                    }
                } else {
                    std::cerr << "Error sending packet, skipping corrupted frame.\n";
                }
            }
            av_packet_unref(&packet);
        }
    }
    av_frame_free(&frame);
    return nullptr;
}

void videoStream::cleanupCamera() {
    if (m_decoderContext) {
        avcodec_free_context(&m_decoderContext);
        m_decoderContext = nullptr;
    }
    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
        m_formatContext = nullptr;
    }
    std::cout << "Camera resources released\n";
}




void videoStream::listenForKeyPress() {
    while (m_recording.load()) {
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'Q' || ch == 'q') {
                m_recording.store(false);

                break;
            }
        }
    }
}


Box readBox(const uint8_t* data, size_t& offset, size_t dataSize) {
    if (offset + 8 > dataSize) {
        throw std::runtime_error("Incomplete box header");
    }

    Box box;
    box.size = ntohl(*reinterpret_cast<const uint32_t*>(data + offset));
    offset += 4;

    char type[5] = {0};
    std::memcpy(type, data + offset, 4);
    box.type = std::string(type);
    offset += 4;

    if (offset + box.size - 8 > dataSize) {
        throw std::runtime_error("Incomplete box data");
    }

    box.data.resize(box.size - 8);
    std::memcpy(box.data.data(), data + offset, box.data.size());
    offset += box.data.size();

    return box;
}
std::vector<uint8_t> videoStream::filterAtoms(const std::vector<uint8_t>& packet) {
    std::vector<uint8_t> filtered_packet;
    size_t offset = 0;

    while (offset < packet.size()) {
        try {
            Box box = readBox(packet.data(), offset, packet.size());
            if (box.type != "ftyp" && box.type != "moov") {
                uint32_t size = htonl(box.size);
                filtered_packet.insert(filtered_packet.end(), reinterpret_cast<uint8_t*>(&size), reinterpret_cast<uint8_t*>(&size) + 4);
                filtered_packet.insert(filtered_packet.end(), box.type.begin(), box.type.end());
                filtered_packet.insert(filtered_packet.end(), box.data.begin(), box.data.end());
            }
        } catch (const std::exception& e) {
            std::cerr << "Error reading box: " << e.what() << std::endl;
            break;
        }
    }

    return filtered_packet;
}

void videoStream::sendLiveVideoToClient() {
    std::cout << "Starting live video to HTML5 client\n";
    int width = 1280, height = 720;

    VideoStreamEncoder::Params params;
    params.width = width;
    params.height = height;
    params.fps = 30.0;
    params.bitrate = 400000;
    params.preset = "medium";
    params.crf = 23;
    params.src_format = AV_PIX_FMT_RGB24;
    params.dst_format = AV_PIX_FMT_YUV420P;

    //Create encoder instance
    VideoStreamEncoder encoder;

    // Open encoder
    if (!encoder.Open(params)) {
        std::cerr << "Failed to open encoder\n";
        return; // Exit if encoder fails to open
    }
    //getchar();

    if (!initializeCamera()) {
        std::cerr << "Failed to initialize camera\n";
        return; // Exit if camera fails to initialize
    } else {
        std::cout << "Camera initialized successfully\n";
    }

    std::thread keyListener(&videoStream::listenForKeyPress, this);

    // Create WebSocket server instance
    VideoStreamSocket server;
    std::thread serverThread([&server]() {
        server.run(9002);
    });


    // Frame queue
    ThreadSafeQueue<unsigned char*> frameQueue;

    // Combined thread for reading frames and rendering
    std::thread frameReaderAndRenderer([&](){
        //std::cout << "Inside frameReaderAndRenderer thread\n";
        while (m_recording.load()) {
            unsigned char* data = getFrameData(width, height);
            if (data) {
                //m_pGUIptr->RenderFrame(m_pGUIptr->getPreviewWindow(), data, width, height);
                notifyObserver(data, width, height);
                frameQueue.push(data);
            }
        }
        std::cout << "Ends frameReaderAndRenderer thread\n";
        cleanupCamera();
        
    });
    
    // Thread to encode frames
    std::thread frameEncoder([&]() {
        while (m_recording.load() || !frameQueue.empty()) {
            unsigned char* data;
            if (frameQueue.pop(data)) {
                if (!encoder.Write(data)) {
                    std::cerr << "Failed to write frame to encoder\n";
                }
                av_free(data);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
            }
        }
        std::cout << "Ends frameEncoder thread\n";

        encoder.Close();
    });
    // Thread to send encoded data to WebSocket server
    std::thread dataSender([&](){
        try {
            std::unique_lock<std::mutex> lock(server.m_mutex);
            server.m_cv.wait(lock, [&server] { return server.m_client_connected; });
            lock.unlock(); // Release the lock before entering the loop

            while (m_recording.load() || !encoder.isencodedFramesQueueEmpty()) {
                std::vector<uint8_t> encodedpacket;
                std::vector<uint8_t> filtered_packet;
                if (encoder.getEncodedFrame(encodedpacket)) {
                    if (!m_initialization_sent) {
                        std::cout << "Sending initialization data" << std::endl;
                        m_initialization_sent = true;
                        server.send_video_data(encodedpacket);
                    } else {
                        filtered_packet = filterAtoms(encodedpacket);
                        server.send_video_data(filtered_packet);
                    }
                    std::cout << "encoded packet size:" << encodedpacket.size() << " filtered packet size:" << filtered_packet.size() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception in dataSender thread: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in dataSender thread" << std::endl;
        }
    });
    
    


    keyListener.join();
    frameReaderAndRenderer.join();
    frameEncoder.join();
    dataSender.join();
    serverThread.join();

    
    std::cerr << "Video capture finished" << std::endl;
}


int videoStream::videoCaptureAndEncoding() {
    std::cout << "Starting Video capture and encoding\n";
    int width = 1280, height = 720;

    FFmpegEncoder::Params params;
    params.width = width;
    params.height = height;
    params.fps = 30.0;
    params.bitrate = 400000;
    params.preset = "medium";
    params.crf = 23;
    params.src_format = AV_PIX_FMT_RGB24;
    params.dst_format = AV_PIX_FMT_YUV420P;

    // Create encoder instance
    FFmpegEncoder encoder;

    // Open encoder
    const char* env_filepath = std::getenv("FILE_PATH");
    env_filepath?std::cout<<env_filepath<<std::endl:std::cout<<"mp4 file path not found"<<std::endl;
    if (!encoder.Open(env_filepath, params)) {
        std::cerr << "Failed to open encoder\n";
        return -1;
    }

    if (!initializeCamera()) {
        std::cerr << "Failed to initialize camera\n";
        return -1;
    } else {
        std::cout << "Camera initialized successfully\n";
    }

    std::thread keyListener(&videoStream::listenForKeyPress, this);
    ThreadSafeQueue<unsigned char*> frameQueue;

    // Combined thread for reading frames and rendering
    std::thread frameReaderAndRenderer([&](){
        while (m_recording.load()) {
            unsigned char* data = getFrameData(width, height);
            if (data) {
                //std::cout << "Frame captured: " << width << "x" << height << std::endl;
                // Render the frame
                
                //m_pGUIptr->RenderFrame(m_pGUIptr->getPreviewWindow(), data, width, height);
                notifyObserver(data, width, height);

                // Push the data to the encoding queue
                frameQueue.push(data);
            } else {
                //std::cerr << "Failed to capture frame\n";
            }
        }
        std::cout << "exiting frameReaderAndRenderer thread\n";
    });

    // Thread to encode frames
    std::thread frameEncoder([&]() {
        while (m_recording.load() || !frameQueue.empty()) {
            unsigned char* data;
            if (frameQueue.pop(data)) {
                if (!encoder.Write(data)) {
                    std::cerr << "Failed to write frame to encoder\n";
                }
                av_free(data);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        std::cout << "exiting frameEncoder thread\n";
    });

    keyListener.join();
    frameReaderAndRenderer.join();
    frameEncoder.join();
    encoder.Close();
    cleanupCamera();

    std::cout << "videocapture finished" << std::endl;
    return 0;
}


void videoStream::playVideo(const char* filename) {
    AVFormatContext* formatContext = nullptr;
    AVCodecContext* codecContext = nullptr;
    const AVCodec* codec = nullptr;
    AVFrame* frame = av_frame_alloc();
    AVPacket packet;

    if (avformat_open_input(&formatContext, filename, nullptr, nullptr) < 0) {
        std::cerr << "Could not open video file." << std::endl;
        return;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream information." << std::endl;
        avformat_close_input(&formatContext);
        return;
    }

    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            codec = avcodec_find_decoder(formatContext->streams[i]->codecpar->codec_id);
            if (!codec) {
                std::cerr << "Could not find codec." << std::endl;
                avformat_close_input(&formatContext);
                return;
            }
            codecContext = avcodec_alloc_context3(codec);
            if (!codecContext) {
                std::cerr << "Could not allocate codec context." << std::endl;
                avformat_close_input(&formatContext);
                return;
            }
            if (avcodec_parameters_to_context(codecContext, formatContext->streams[i]->codecpar) < 0) {
                std::cerr << "Could not copy codec parameters." << std::endl;
                avcodec_free_context(&codecContext);
                avformat_close_input(&formatContext);
                return;
            }
            if (avcodec_open2(codecContext, codec, nullptr) < 0) {
                std::cerr << "Could not open codec." << std::endl;
                avcodec_free_context(&codecContext);
                avformat_close_input(&formatContext);
                return;
            }
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "Could not find video stream." << std::endl;
        avformat_close_input(&formatContext);
        return;
    }

    AVRational timeBase = formatContext->streams[videoStreamIndex]->time_base;
    auto startTime = std::chrono::high_resolution_clock::now();

    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            if (avcodec_send_packet(codecContext, &packet) == 0) {
                while (avcodec_receive_frame(codecContext, frame) == 0) {
                    int width = frame->width;
                    int height = frame->height;
                    AVFrame* rgbFrame = av_frame_alloc();
                    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 1);
                    unsigned char* rgbBuffer = (unsigned char*)av_malloc(numBytes * sizeof(unsigned char));
                    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, rgbBuffer, AV_PIX_FMT_RGB24, width, height, 1);
                    struct SwsContext* swsCtx = sws_getContext(
                        frame->width, frame->height, codecContext->pix_fmt,
                        width, height, AV_PIX_FMT_RGB24,
                        SWS_BILINEAR, nullptr, nullptr, nullptr
                    );
                    sws_scale(swsCtx, frame->data, frame->linesize, 0, frame->height, rgbFrame->data, rgbFrame->linesize);
                    //m_pGUIptr->RenderFrame(m_pGUIptr->getPreviewWindow(), rgbFrame->data[0], width, height);
                    notifyObserver(rgbFrame->data[0], width, height);
                    av_free(rgbBuffer);
                    av_frame_free(&rgbFrame);
                    sws_freeContext(swsCtx);

                    // Calculate the delay based on the frame's presentation timestamp (PTS)
                    int64_t pts = av_rescale_q(frame->pts, timeBase, AV_TIME_BASE_Q);
                    auto currentTime = std::chrono::high_resolution_clock::now();
                    auto elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime).count();
                    int64_t delay = pts - elapsedTime;
                    if (delay > 0) {
                        std::this_thread::sleep_for(std::chrono::microseconds(delay));
                    }
                }
            }
            av_packet_unref(&packet);
        }
    }

    av_frame_free(&frame);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
}

