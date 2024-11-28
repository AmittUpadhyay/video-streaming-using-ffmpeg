#ifndef VIDEOSTREAMENCODER_HPP
#define VIDEOSTREAMENCODER_HPP

#include <vector>
#include <thread>
#include <CThreadSafeQueue.hpp>


/**
 * @class VideoStreamEncoder
 * @brief Class for encoding video streams.
 *
 * This class handles the encoding of video streams, providing methods to open, write, and close the encoder.
 */
class VideoStreamEncoder {
public:
    /**
     * @brief Constructor for VideoStreamEncoder.
     */
    VideoStreamEncoder();

    /**
     * @brief Destructor for VideoStreamEncoder.
     */
    ~VideoStreamEncoder();

    /**
     * @struct Params
     * @brief Parameters for configuring the video encoder.
     *
     * This structure holds the configuration parameters for the video encoder, including frame dimensions, frame rate, bitrate, and pixel formats.
     */
    struct Params
    {
        uint32_t width; ///< Width of the video frame.
        uint32_t height; ///< Height of the video frame.
        double fps; ///< Frames per second.
        uint32_t bitrate; ///< Bitrate for encoding.
        const char *preset; ///< Preset for encoding quality and speed.
        uint32_t crf; ///< Constant Rate Factor (0–51).
        enum AVPixelFormat src_format; ///< Source pixel format.
        enum AVPixelFormat dst_format; ///< Destination pixel format.
    };

    /**
     * @brief Open the video encoder with specified parameters.
     *
     * @param params The parameters for configuring the encoder.
     * @return True if the encoder was successfully opened, false otherwise.
     */
    bool Open(const Params& params);

    /**
     * @brief Close the video encoder.
     */
    void Close();

    /**
     * @brief Write raw video data to the encoder.
     *
     * @param data Pointer to the raw video data.
     * @return True if the data was successfully written, false otherwise.
     */
    bool Write(const unsigned char *data);

    /**
     * @brief Get the encoded video frame.
     *
     * @param frame Vector to store the encoded frame data.
     * @return True if the frame was successfully retrieved, false otherwise.
     */
    bool getEncodedFrame(std::vector<uint8_t>& frame);

    /**
     * @brief Check if the encoded frames queue is empty.
     *
     * @return True if the queue is empty, false otherwise.
     */
    bool isencodedFramesQueueEmpty();

private:
    /**
     * @brief Flush the remaining packets in the encoder.
     *
     * @return True if the packets were successfully flushed, false otherwise.
     */
    bool FlushPackets();

    /**
     * @brief Remux the video data.
     *
     * @param data Pointer to the video data.
     * @param size Size of the video data.
     * @return True if the data was successfully remuxed, false otherwise.
     */
    bool remuxVideo(uint8_t* data, int size);

    /**
     * @struct Context
     * @brief Internal context for the video encoder.
     *
     * This structure holds the internal context for the video encoder, including format and codec contexts, stream, and frame information.
     */
    struct Context {
        AVFormatContext *format_context = nullptr;
        AVCodecContext *codec_context = nullptr;
        AVStream *stream = nullptr;
        const AVCodec *codec = nullptr;
        AVFrame *frame = nullptr;
        SwsContext *sws_context = nullptr;
        int frame_index = 0;
    } mContext;

    bool mIsOpen = false; ///< Flag indicating if the encoder is open.
    ThreadSafeQueue<std::vector<uint8_t>> encodedFramesQueue; ///< Queue for storing encoded frames.
    std::vector<uint8_t> m_initSegment; ///< Initial segment of the encoded video.
};

#endif // VIDEOSTREAMENCODER_HPP