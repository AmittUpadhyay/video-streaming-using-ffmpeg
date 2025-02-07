#pragma once
#ifndef VIDEOSTREAM_HPP
#define VIDEOSTREAM_HPP
#include <queue>
#include <mutex>
#include <condition_variable>


extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include <CThreadSafeQueue.hpp>
#include<CObserver.hpp>


struct Box {
    uint32_t size;
    std::string type;
    std::vector<uint8_t> data;
};
class VideoCaptureGUI;


/**
 * @class videoStream
 * @brief Class responsible for capturing video frames after initializing the webcam and encoding the frame data to the appropriate format.
 *
 * This class uses ffmpeg APIs to initialize the webcam, capture video frames, and encode the frame data.
 * It also provides functionalities to control the program's lifecycle, play videos, and clean up resources.
 */
class videoStream
{
    /**
     * @brief Open and initialize the camera using ffmpeg APIs.
     *
     * This method sets up the camera for capturing video frames.
     * @return true if the camera was successfully initialized, false otherwise.
     */
    bool initializeCamera();

    /**
     * @brief Capture a single frame of video data.
     *
     * After initializing the camera, this method allocates memory for a frame, reads the frame data,
     * stores it in a char buffer, and returns the buffer containing the single frame data.
     *
     * @param width Reference to an integer where the frame width will be stored.
     * @param height Reference to an integer where the frame height will be stored.
     * @return A char buffer containing the frame data.
     */
    unsigned char* getFrameData(int& width, int& height);

    /**
     * @brief Listen for a key press to control the exit or end of life of the program.
     *
     * This method waits for a key press from the keyboard to terminate the program.
     */
    void listenForKeyPress();

    /**
     * @brief Clean up camera resources allocated for reading frames from the camera.
     *
     * This method releases any resources that were allocated for the camera.
     */
    void cleanupCamera();
    /**
        @brief Function to filter atoms from a given packet
        @param packet: A vector containing the atoms to be filtered
        @return: A vector containing the filtered atoms
     */
    std::vector<uint8_t> filterAtoms(const std::vector<uint8_t>& packet);

public:

    std::atomic<bool> m_recording{false}; ///< boolean to indicate recording status
    /**
     * @brief Constructor for the videoStream class.
     *
     * Initializes the videoStream object.
     */
    videoStream();

    /**
     * @brief Remux the final mp4 encoded video to ffmp4 format.
     *
     * This method takes an input filename and an output filename to remux the video.
     *
     * @param inputFilename The name of the input file.
     * @param outputFilename The name of the output file.
     * @return true if the remuxing was successful, false otherwise.
     */
    bool remuxVideo(const char* inputFilename, const char* outputFilename);

    /**
     * @brief Sets the observer for the video stream.
     * 
     * This function assigns an observer to the video stream, which will be notified of various events.
     * 
     * @param observer A pointer to the Observer object to be set.
     */
    void setObserver(Observer* observer) {
    m_observer = observer;
    }

    /**
     * @brief Notifies the observer with the provided video frame data.
     *
     * This function is called to update the observer with new video frame data.
     * If an observer is registered, it will call the observer's update method
     * with the provided frame data, width, and height.
     *
     * @param data Pointer to the video frame data.
     * @param width Width of the video frame.
     * @param height Height of the video frame.
     */
    void notifyObserver(unsigned char* data, uint32_t width, uint32_t height) {
        if (m_observer) {
            m_observer->update(data, width, height);
        }
    }

    /**
     * @brief Play the latest video file (.mp4 or .fmp4).
     *
     * This method plays the specified video file.
     *
     * @param filename The name of the video file to play.
     */
    void playVideo(const char* filename);

    /**
     * @brief Capture video data and encode it.
     *
     * This method captures frame data, renders it to the preview window, and encodes the captured data to an mp4 container.
     *
     * @return An integer indicating the success or failure of the operation.
     */
    int videoCaptureAndEncoding();
    /**
     * @brief Send live video feed to a client.
     *
     * This function captures live video data, processes it, and transmits the video feed to a connected client.
     * It handles the entire process of capturing, encoding, and sending the video stream.
     *
     * @return void
     */
    void sendLiveVideoToClient();


    /**
     * @brief Destructor for the videoStream class.
     *
     * Cleans up resources used by the videoStr1eam object.
     */
    ~videoStream();
    private:
    bool m_initialization_sent = false; ///< boolean to check if initialization is sent already.
    AVFormatContext* m_formatContext{nullptr}; ///< Pointer variable for video format context.
    AVCodecContext* m_decoderContext{nullptr}; ///< Pointer variable for decoder format context.
    int m_videoStreamIndex = 0; ///< Integer for video stream index.
    Observer *m_observer = nullptr;  ///< @brief Pointer to an Observer object.

};
#endif