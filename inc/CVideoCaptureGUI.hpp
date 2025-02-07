#pragma once
#include<windows.h>
#include<thread>
#include<vector>

#ifndef VIDEOCAPTUREGUI_HPP
#define VIDEOCAPTUREGUI_HPP

class videoStream;

/**
 * @class VideoCaptureGUI
 * @brief Manages the graphical user interface for video capture.
 *
 * This class handles the creation and management of the GUI elements
 * for video capture, including rendering frames and interacting with
 * video streaming objects.
 */
class VideoCaptureGUI:public Observer {
public:
    /**
     * @brief Constructs a VideoCaptureGUI object.
     *
     * Initializes the GUI with the provided instance handle.
     *
     * @param hInstance Handle to the instance of the application.
     */
    VideoCaptureGUI(HINSTANCE hInstance);

        /**
     * @brief Destroys the VideoCaptureGUI object.
     *
     * Cleans up resources used by the GUI.
     */
    ~VideoCaptureGUI();

    /**
     * @brief Displays the GUI window.
     *
     * Shows the GUI window with the specified command show parameter.
     *
     * @param nCmdShow Specifies how the window is to be shown.
     */
    void update(unsigned char* update , uint32_t width, uint32_t height) override;
    void Show(int nCmdShow);

     /**
     * @brief Window procedure for handling messages.
     *
     * Static callback function to process messages sent to the window.
     *
     * @param hwnd Handle to the window.
     * @param uMsg Message identifier.
     * @param wParam Additional message information.
     * @param lParam Additional message information.
     * @return Result of the message processing.
     */
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

     /**
     * @brief Renders a video frame in the preview window.
     *
     * Draws the provided frame data in the specified window.
     *
     * @param hWnd Handle to the window where the frame will be rendered.
     * @param data Pointer to the frame data.
     * @param width Width of the frame.
     * @param height Height of the frame.
     */
    void RenderFrame(HWND hWnd, unsigned char* data, int width, int height);
    void DecodeAndRenderFrame(HWND hWnd, const std::vector<uint8_t>& encodedData, int width, int height);

    /**
     * @brief Gets the video stream object.
     *
     * Returns a pointer to the video stream object associated with the GUI.
     *
     * @return Pointer to the video stream object.
     */
    inline videoStream* getVideoStream()
    {
        return m_videoStreamPtr;
    }

    /**
     * @brief Gets the preview window handle.
     *
     * Returns the handle to the preview window.
     *
     * @return Handle to the preview window.
     */
    inline HWND getPreviewWindow()
    {
        return hPreviewWindow;
    }
    /**
     * @brief Sets the video stream object.
     *
     * Assigns the provided video stream object to the GUI.
     *
     * @param videoStreamPtr Pointer to the video stream object to be set.
     */
    inline void setVideoStreamingObject(videoStream* videoStreamPtr)
    {
        m_videoStreamPtr = videoStreamPtr;
    }
    /**
     * @brief Thread for handling video streaming.
     *
     * Manages the video streaming process in a separate thread.
     */
    std::thread m_videoThread;

private:
    HWND hwnd; ///< Handle to the main window.
    HWND hPreviewWindow; ///< Handle to the preview window.
    HWND hStartStopButton; ///< Handle to the start/stop button.
    HWND hRemuxButton; ///< Handle to the remux button.
    HWND hPlayButton; ///< Handle to the play button.
    videoStream* m_videoStreamPtr{nullptr}; ///< Pointer to the video stream object.
    HWND hWebSocketButton; ///< Handle to the start websocket button.
    
    
};

#endif // VIDEOCAPTUREGUI_HPP
