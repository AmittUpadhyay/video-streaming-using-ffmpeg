#pragma once
extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavutil/frame.h>
#include <stdint.h>
#include <libavutil\pixfmt.h>
}


/**
 * @class FFmpegEncoder
 * @brief Class responsible for encoding video frames using FFmpeg.
 *
 * This class provides functionalities to initialize the encoder, encode video frames, and manage resources.
 */
class FFmpegEncoder
{
public:
    /**
     * @struct Params
     * @brief Structure to hold encoding parameters.
     *
     * This structure contains various parameters required for encoding, such as resolution, frame rate, bitrate, and pixel formats.
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
     * @brief Default constructor for FFmpegEncoder.
     */
    FFmpegEncoder() = default;

    /**
     * @brief Constructor that initializes the encoder.
     *
     * This constructor calls the Open method with the provided filename and parameters to initialize the encoder.
     *
     * @param filename The name of the output file.
     * @param params The encoding parameters.
     */
    FFmpegEncoder(const char *filename, const Params &params);

    /**
     * @brief Destructor that cleans up resources.
     *
     * This destructor calls the Close method to release all allocated resources.
     */
    ~FFmpegEncoder();

    /**
     * @brief Opens the encoder and sets up the necessary FFmpeg structures for encoding video.
     *
     * This method allocates the output format context, finds and sets up the H.264 encoder, creates a new stream for the video,
     * allocates and configures the codec context with parameters like bitrate, resolution, frame rate, and pixel format,
     * initializes the frame and conversion context, and opens the output file and writes the header.
     *
     * @param filename The name of the output file.
     * @param params The encoding parameters.
     * @return true if the encoder was successfully opened, false otherwise.
     */
    bool Open(const char *filename, const Params &params);

    /**
     * @brief Closes the encoder and releases all allocated resources.
     *
     * This method sends a null frame to flush the encoder, writes the trailer to finalize the file, closes the output file,
     * and frees the conversion context, frame, codec context, and format context.
     */
    void Close();

    /**
     * @brief Encodes a frame of video data.
     *
     * This method makes the frame writable, converts the input data to the desired pixel format, sends the frame to the encoder,
     * and flushes the encoded packets to the output file.
     *
     * @param data The input frame data.
     * @return true if the frame was successfully encoded, false otherwise.
     */
    bool Write(const unsigned char *data);

    /**
     * @brief Checks if the encoder is currently open.
     *
     * @return true if the encoder is open, false otherwise.
     */
    bool IsOpen() const;

private:
    /**
     * @brief Flushes the encoded packets from the encoder to the output file.
     *
     * This method receives encoded packets from the encoder, rescales the packet timestamps, writes the packets to the output file,
     * and unreferences the packet to free its resources.
     *
     * @return true if the packets were successfully flushed, false otherwise.
     */
    bool FlushPackets();

private:
    bool mIsOpen = false; ///< Indicates whether the encoder is open.

    /**
     * @struct Context
     * @brief Structure to hold FFmpeg context information.
     *
     * This structure contains pointers to various FFmpeg contexts and the frame index.
     */
    struct Context
    {
        struct AVFormatContext *format_context = nullptr; /*Pointer to the format context.*/
        struct AVStream *stream = nullptr; /*Pointer to the stream */
        struct AVCodecContext *codec_context = nullptr; /* Pointer to the codec context.*/
        struct AVFrame *frame = nullptr; /*Pointer to the frame.*/
        struct SwsContext *sws_context = nullptr; /*Pointer to the software scaling context.*/
        const struct AVCodec *codec = nullptr; ///< Pointer to the codec.      
        uint32_t frame_index = 0; ///< Index of the current frame.
    };

    Context mContext = {}; ///< Instance of the Context structure.
};
