#include <windows.h>
#include<iostream>
#include<thread>
#include <mutex>
#include <CStreamVideo.hpp>
#include <fstream>
#include <CVideoCaptureGUI.hpp>
#include <CWebSocketServer.hpp>
extern "C" {

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavcodec/codec.h>
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    // Allocate a console for this application
    AllocConsole();
    // Redirect standard output to the console
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    
    auto vsPtr = std::make_unique<videoStream>();
    auto uiPtr = std::make_unique<VideoCaptureGUI>(hInstance);

   if (vsPtr && uiPtr) {
        vsPtr->setObserver(uiPtr.get()); 
   }

    std::cout << "Console window created:" <<nCmdShow << std::endl; 
    uiPtr->Show(nCmdShow);

    return 0;
}

VideoCaptureGUI::VideoCaptureGUI(HINSTANCE hInstance) {
    const wchar_t CLASS_NAME[] = L"Sample Window Class";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);
     

    hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Video Capture Control",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 600,
        NULL,
        NULL,
        hInstance,
        this
    );

    if (hwnd == NULL) {
        std::cerr << "Failed to create window." << std::endl;
        return;
    }

    hStartStopButton = CreateWindowW(
        L"BUTTON",
        L"Start",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        50, 50, 100, 30,
        hwnd,
        (HMENU)1,
        hInstance,
        NULL
    );

    hRemuxButton = CreateWindowW(
        L"BUTTON",
        L"Remux",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        160, 50, 100, 30,
        hwnd,
        (HMENU)3,
        hInstance,
        NULL
    );

    hPlayButton = CreateWindowW(
        L"BUTTON",
        L"Play",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        270, 50, 100, 30,
        hwnd,
        (HMENU)2,
        hInstance,
        NULL
    );

    hWebSocketButton = CreateWindowW(
    L"BUTTON",
    L"Start WebSocket",
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
    380, 50, 150, 30,
    hwnd,
    (HMENU)4,
    hInstance,
    NULL
);

    hPreviewWindow = CreateWindowExW(
        0,
        L"STATIC",
        NULL,
        WS_CHILD | WS_VISIBLE | SS_BLACKFRAME,
        50, 100, 500, 280,
        hwnd,
        NULL,
        hInstance,
        NULL
    );
}

VideoCaptureGUI::~VideoCaptureGUI() {
    // Cleanup if necessary

}

void VideoCaptureGUI::update(unsigned char* data , uint32_t width, uint32_t height)
{
    RenderFrame(getPreviewWindow(), data, width, height);
}

void VideoCaptureGUI::Show(int nCmdShow) {
    ShowWindow(hwnd, nCmdShow);
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK VideoCaptureGUI::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    VideoCaptureGUI* pThis = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (VideoCaptureGUI*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (VideoCaptureGUI*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    const char* env_filepath = std::getenv("FILE_PATH");
    size_t len = std::strlen(env_filepath) + 1; // +1 for the null terminator
    wchar_t* wideStr_env_filepath = new wchar_t[len];
    std::mbstowcs(wideStr_env_filepath, env_filepath, len);

    const char* env_muxed_filepath = std::getenv("MUXED_FILE_PATH");
    size_t muxlen = std::strlen(env_filepath) + 1; // +1 for the null terminator
    wchar_t* wideStr_env_muxed_filepath = new wchar_t[muxlen];
    std::mbstowcs(wideStr_env_muxed_filepath, env_filepath, muxlen);

    if (pThis) {
        switch (uMsg) {
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;

            case WM_COMMAND:
                //mutex to protect shared resources
                std::mutex mtx;

                if (LOWORD(wParam) == 1) { // Start/Stop Button ID
                    std::cout << "Start/Stop button clicked" << std::endl;
                    std::lock_guard<std::mutex> lock(mtx); // Lock the mutex
                    if (pThis->getVideoStream()->m_recording.load()) {
                        std::cout << "Stopping recording" << std::endl;
                        pThis->getVideoStream()->m_recording.store(false);
                        if (pThis->m_videoThread.joinable()) {
                            std::cout << "Joining video thread" << std::endl;
                            try {
                                pThis->m_videoThread.join();
                            } catch (const std::exception& e) {
                                std::cerr << "Exception while joining thread: " << e.what() << std::endl;
                            }
                        }
                        SetWindowTextW(pThis->hStartStopButton, L"Start");
                    } else {
                        std::cout << "Starting recording" << std::endl;
                        pThis->getVideoStream()->m_recording.store(true);
                        if (!pThis->m_videoThread.joinable()) {
                            std::cout << "Starting new video thread" << std::endl;
                            try {
                                pThis->m_videoThread = std::thread(&videoStream::videoCaptureAndEncoding, pThis->getVideoStream()); // Start the video capture in a new thread
                            } catch (const std::exception& e) {
                                std::cerr << "Exception while starting thread: " << e.what() << std::endl;
                            }
                        }
                        SetWindowTextW(pThis->hStartStopButton, L"Stop");
                    }
                }



                else if (LOWORD(wParam) == 2) { // Play Button ID                  
                    
                    HANDLE hFile1 = CreateFileW(wideStr_env_filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    HANDLE hFile2 = CreateFileW(wideStr_env_muxed_filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    FILETIME ft1, ft2;
                    bool file1Exists = (hFile1 != INVALID_HANDLE_VALUE && GetFileTime(hFile1, NULL, NULL, &ft1));
                    bool file2Exists = (hFile2 != INVALID_HANDLE_VALUE && GetFileTime(hFile2, NULL, NULL, &ft2));

                    if (file1Exists && file2Exists) {
                        if (CompareFileTime(&ft1, &ft2) == 1) {
                            pThis->getVideoStream()->playVideo(env_filepath);
                        } else {
                            pThis->getVideoStream()->playVideo(env_muxed_filepath);
                        }
                    } else if (file1Exists) {
                        pThis->getVideoStream()->playVideo(env_filepath);
                    } else if (file2Exists) {
                        pThis->getVideoStream()->playVideo(env_muxed_filepath);
                    } else {
                        MessageBoxW(hwnd, L"No video files found.", L"Error", MB_OK);
                    }

                    if (hFile1 != INVALID_HANDLE_VALUE) CloseHandle(hFile1);
                    if (hFile2 != INVALID_HANDLE_VALUE) CloseHandle(hFile2);
                } else if (LOWORD(wParam) == 3) { // Remux Button ID
                                      
                    if (pThis->getVideoStream()->remuxVideo(env_filepath, env_muxed_filepath)) {
                        MessageBoxW(hwnd, L"Remuxing completed successfully!", L"Success", MB_OK);
                    } else {
                        MessageBoxW(hwnd, L"Remuxing failed.", L"Error", MB_OK);
                    }
                }
                 else if (LOWORD(wParam) == 4) { // Start WebSocket Button ID
                
                std::cout << "Starting WebSocket server ..." << std::endl;
                     auto webSocketServer = std::make_unique<WebSocketServer>(8080);
                    std::thread(&WebSocketServer::start, webSocketServer.get()).detach();
            }
                return 0;
        }
    }
    delete wideStr_env_filepath;
    delete wideStr_env_muxed_filepath;

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}






void VideoCaptureGUI::RenderFrame(HWND hWnd, unsigned char* data, int width, int height) {
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
void VideoCaptureGUI::DecodeAndRenderFrame(HWND hWnd, const std::vector<uint8_t>& encodedData, int width, int height) {
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = const_cast<uint8_t*>(encodedData.data());
    packet.size = encodedData.size();

    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec\n";
        return;
    }

    AVFrame* frame = av_frame_alloc();
    AVFrame* rgbFrame = av_frame_alloc();
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, width, height, 32);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer, AV_PIX_FMT_RGB24, width, height, 32);

    struct SwsContext* swsContext = sws_getContext(width, height, codecContext->pix_fmt, width, height, AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (avcodec_send_packet(codecContext, &packet) == 0) {
        if (avcodec_receive_frame(codecContext, frame) == 0) {
            sws_scale(swsContext, (uint8_t const* const*)frame->data, frame->linesize, 0, height, rgbFrame->data, rgbFrame->linesize);
            RenderFrame(hWnd, rgbFrame->data[0], width, height);
        }
    }

    av_free(buffer);
    av_frame_free(&rgbFrame);
    av_frame_free(&frame);
    avcodec_free_context(&codecContext);
    sws_freeContext(swsContext);
}