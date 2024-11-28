#include <windows.h>
#include <thread>
#include <conio.h> 
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <string>
#include <atomic> 
#include <fstream>
#include <CFFmpegEncoder.hpp>


extern "C" {
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
}


#include<CStreamVideo.hpp>
// Global variables
std::atomic<bool> recording(false);
std::thread videoThread;
HWND hPreviewWindow;

void RenderFrame(HWND hWnd, unsigned char* data, int width, int height) {
    HDC hdc = GetDC(hWnd);
    if (!hdc) {
        std::cerr << "Could not get device context\n";
        return;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Negative to indicate top-down bitmap
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Get the dimensions of the subwindow
    RECT rect;
    GetClientRect(hWnd, &rect);
    int destWidth = rect.right - rect.left;
    int destHeight = rect.bottom - rect.top;

    // Maintain aspect ratio
    float aspectRatio = static_cast<float>(width) / height;
    if (destWidth > destHeight * aspectRatio) {
        destWidth = static_cast<int>(destHeight * aspectRatio);
    } else {
        destHeight = static_cast<int>(destWidth / aspectRatio);
    }

    // Center the video frame within the subwindow
    int offsetX = (rect.right - rect.left - destWidth) / 2;
    int offsetY = (rect.bottom - rect.top - destHeight) / 2;

    // Use a higher-quality scaling method
    SetStretchBltMode(hdc, HALFTONE);
    StretchDIBits(
        hdc,
        offsetX, offsetY, destWidth, destHeight, // Destination dimensions
        0, 0, width, height,                    // Source dimensions
        data,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );

    ReleaseDC(hWnd, hdc);
}




videoStream::videoStream(): m_formatContext{nullptr}, m_decoderContext{nullptr}, m_videoStreamIndex{-1}
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
    while (recording) {
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
    avcodec_free_context(&m_decoderContext);
    avformat_close_input(&m_formatContext);
}


void videoStream::listenForKeyPress() {
    while (recording) {
        if (_kbhit()) {
            char ch = _getch();
            if (ch == 'Q' || ch == 'q') {
                recording = false;
                break;
            }
        }
    }
}



int videoStream::videoCaptureAndEncoding() {
    videoStream app;
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
    if (!encoder.Open("output.mp4", params)) {
        std::cerr << "Failed to open encoder\n";
        return -1;
    }

    if (!app.initializeCamera()) {
        std::cerr << "Failed to initialize camera\n";
        return -1;
    }

    std::thread keyListener(&videoStream::listenForKeyPress, &app);
    ThreadSafeQueue<unsigned char*> frameQueue;

    // Combined thread for reading frames and rendering
    std::thread frameReaderAndRenderer([&]() {
        while (recording) {
            unsigned char* data = app.getFrameData(width, height);
            if (data) {
                // Render the frame
                RenderFrame(hPreviewWindow, data, width, height);

                // Push the data to the encoding queue
                frameQueue.push(data);
            }
        }
    });

    // Thread to encode frames
    std::thread frameEncoder([&]() {
        while (recording || !frameQueue.empty()) {
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
    });

    keyListener.join();
    frameReaderAndRenderer.join();
    frameEncoder.join();
    encoder.Close();

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
                    RenderFrame(hPreviewWindow, rgbFrame->data[0], width, height);
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



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    /*
    // Allocate a console for this application
    AllocConsole();
    // Redirect standard output to the console
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    std::cout << "Console window created" << std::endl; */

    const wchar_t CLASS_NAME[] = L"Sample Window Class";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Video Capture Control",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    // Adjust button positions for horizontal layout
    HWND hStartStopButton = CreateWindowW(
        L"BUTTON",
        L"Start",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        50, 50, // X, Y position
        100, 30, // Width, Height
        hwnd,
        (HMENU)1,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    HWND hRemuxButton = CreateWindowW(
        L"BUTTON",
        L"Remux",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        160, 50, // X, Y position
        100, 30, // Width, Height
        hwnd,
        (HMENU)3,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    HWND hPlayButton = CreateWindowW(
        L"BUTTON",
        L"Play",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        270, 50, // X, Y position
        100, 30, // Width, Height
        hwnd,
        (HMENU)2,
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL
    );

    hPreviewWindow = CreateWindowExW(
        0,
        L"STATIC",
        NULL,
        WS_CHILD | WS_VISIBLE | SS_BLACKFRAME,
        50, 100, 500, 280, // Adjusted position to avoid overlapping with buttons
        hwnd,
        NULL,
        hInstance,
        NULL
    );

    ShowWindow(hwnd, nCmdShow);
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    videoStream app;
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { // Start/Stop Button ID
                if (recording) {
                    recording = false;
                    if (videoThread.joinable()) {
                        videoThread.join();
                    }
                    SetWindowTextW((HWND)lParam, L"Start");
                } else {
                    recording = true;
                    videoThread = std::thread(&videoStream::videoCaptureAndEncoding, &app); // Start the video capture in a new thread
                    SetWindowTextW((HWND)lParam, L"Stop");
                }
            } else if (LOWORD(wParam) == 2) { // Play Button ID
                
                HANDLE hFile1 = CreateFileW(L"output.mp4", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                HANDLE hFile2 = CreateFileW(L"muxed_output.fmp4", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

                FILETIME ft1, ft2;
                bool file1Exists = (hFile1 != INVALID_HANDLE_VALUE && GetFileTime(hFile1, NULL, NULL, &ft1));
                bool file2Exists = (hFile2 != INVALID_HANDLE_VALUE && GetFileTime(hFile2, NULL, NULL, &ft2));

                if (file1Exists && file2Exists) {
                    if (CompareFileTime(&ft1, &ft2) == 1) {
                        app.playVideo("output.mp4");
                    } else {
                        app.playVideo("muxed_output.fmp4");
                    }
                } else if (file1Exists) {
                    app.playVideo("output.mp4");
                } else if (file2Exists) {
                    app.playVideo("muxed_output.fmp4");
                } else {
                    MessageBoxW(hwnd, L"No video files found.", L"Error", MB_OK);
                }

                if (hFile1 != INVALID_HANDLE_VALUE) CloseHandle(hFile1);
                if (hFile2 != INVALID_HANDLE_VALUE) CloseHandle(hFile2);
            } else if (LOWORD(wParam) == 3) { // Remux Button ID
                
                if (app.remuxVideo("output.mp4", "muxed_output.fmp4")) {
                    MessageBoxW(hwnd, L"Remuxing completed successfully!", L"Success", MB_OK);
                } else {
                    MessageBoxW(hwnd, L"Remuxing failed.", L"Error", MB_OK);
                }
            }
            return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
