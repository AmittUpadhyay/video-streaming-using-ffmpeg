
/**
 * @class ThreadSafeQueue
 * @brief A thread-safe queue implementation using a mutex and condition variable.
 *
 * This class provides a thread-safe queue that allows multiple threads to push and pop elements
 * concurrently. It ensures that access to the queue is synchronized using a mutex and condition variable.
 *
 * @tparam T The type of elements stored in the queue.
 */
template <typename T>
class ThreadSafeQueue {
public:
    /**
     * @brief Pushes a value into the queue.
     *
     * This method locks the mutex, pushes the value into the queue, and notifies one waiting thread.
     *
     * @param value The value to be pushed into the queue.
     */
    void push(T value){
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_var_.notify_one();
    }

    /**
     * @brief Pops a value from the queue.
     *
     * This method locks the mutex and waits until the queue is not empty. It then pops the front value
     * from the queue and assigns it to the provided reference.
     *
     * @param value Reference to the variable where the popped value will be stored.
     * @return true if a value was successfully popped from the queue.
     */
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this] { return !queue_.empty(); });
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    /**
     * @brief Checks if the queue is empty.
     *
     * This method locks the mutex and checks if the queue is empty.
     *
     * @return true if the queue is empty, false otherwise.
     */
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_; ///< The underlying queue storing the elements.
    std::mutex mutex_; ///< Mutex for synchronizing access to the queue.
    std::condition_variable cond_var_; ///< Condition variable for notifying waiting threads.
};


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

public:
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
     * @brief Destructor for the videoStream class.
     *
     * Cleans up resources used by the videoStr1eam object.
     */
    ~videoStream();

    void sendLiveVideoToClient();

    AVFormatContext* m_formatContext; ///< Pointer variable for video format context.
    AVCodecContext* m_decoderContext; ///< Pointer variable for decoder format context.
    int m_videoStreamIndex; ///< Integer for video stream index.
};

/**
 * Entry point for a Windows application.
 *
 * This function performs the following tasks:
 * - Registers the window class with a name and the WindowProc function as the window procedure.
 * - Creates a window with the specified class name, title, and dimensions.
 * - Creates two buttons within the window:
 *   - Start/Stop Button: Initially labeled "Start", used to start and stop video recording.
 *   - Play Button: Labeled "Play", used to play the recorded video.
 * - Makes the window visible.
 * - Runs a message loop that retrieves and dispatches messages to the window procedure until a WM_QUIT message is received.
 *
 * @param hInstance Handle to the current instance of the application.
 * @param hPrevInstance Handle to the previous instance of the application (always NULL).
 * @param lpCmdLine Command line for the application.
 * @param nCmdShow Controls how the window is to be shown.
 * @return Exit value of the application.
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

/**
 * Processes messages sent to a window.
 *
 * This function handles the following messages:
 * - WM_DESTROY: Sent when the window is being destroyed. Calls PostQuitMessage(0) to signal the end of the application and returns 0.
 * - WM_COMMAND: Sent when a command is issued, such as a button click. Checks the wParam to determine which button was clicked:
 *   - Start/Stop Button (ID 1): Toggles the recording state. If recording is active, stops the recording and joins the video thread, then changes the button text to "Start". If recording is inactive, starts the recording in a new thread and changes the button text to "Stop".
 *   - Play Button (ID 2): Opens the output.mp4 file using the default media player.
 * - If the message is not handled by these cases, calls DefWindowProcW to handle the default processing.
 *
 * @param hwnd Handle to the window.
 * @param uMsg Message identifier.
 * @param wParam Additional message information.
 * @param lParam Additional message information.
 * @return Result of the message processing.
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/**
 * Renders a video frame onto a specified window.
 *
 * @param hWnd Handle to the window where the frame will be rendered.
 * @param data Pointer to the frame data.
 * @param width Width of the frame.
 * @param height Height of the frame.
 */
void RenderFrame(HWND hWnd, unsigned char* data, int width, int height);


