#pragma once
#ifndef WEBSOCKETSERVER_HPP
#define WEBSOCKETSERVER_HPP

//#define _WINSOCK_DEPRECATED_NO_WARNINGS
//#define _WIN32_WINNT 0x0601



#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "libcrypto.lib")

/**
 * @class WebSocketServer
 * @brief A class to handle WebSocket server operations.
 *
 * This class provides functionalities to start a WebSocket server,
 * handle client connections, and send data frames.
 */
class WebSocketServer {
public:
    /**
     * @brief Constructs a WebSocketServer object.
     *
     * Initializes the server to listen on the specified port.
     *
     * @param port Port number to listen on.
     */
    WebSocketServer(int port);

    /**
     * @brief Destroys the WebSocketServer object.
     *
     * Cleans up resources used by the server.
     */
    ~WebSocketServer();

    /**
     * @brief Starts the WebSocket server.
     *
     * Begins listening for incoming connections and handles them.
     */
    void start();

    void sendData(const std::vector<uint8_t>& data);

private:
    static const std::string WEBSOCKET_MAGIC_STRING; ///< Magic string used in WebSocket handshake.
    int port; ///< Port number the server listens on.
    SOCKET listen_socket; ///< Socket for listening to incoming connections.

    /**
     * @brief Encodes data to Base64 format.
     *
     * @param input Pointer to the data to be encoded.
     * @param length Length of the data to be encoded.
     * @return Base64 encoded string.
     */
    static std::string base64_encode(const unsigned char* input, int length);

    /**
     * @brief Computes the SHA-1 hash of the input string.
     *
     * @param input Input string to be hashed.
     * @return SHA-1 hash of the input string.
     */
    static std::string sha1_hash(const std::string& input);

    /**
     * @brief Sends a WebSocket frame to the client.
     *
     * @param client_socket Socket connected to the client.
     * @param data Data to be sent in the WebSocket frame.
     */
    static void send_websocket_frame(SOCKET client_socket, const std::vector<unsigned char>& data);

    /**
     * @brief Sends FMP4 data to the client.
     *
     * @param client_socket Socket connected to the client.
     * @param file_path Path to the FMP4 file to be sent.
     */
    static void send_fmp4_data(SOCKET client_socket, const std::string& file_path);

    /**
     * @brief Handles an incoming client connection.
     *
     * @param client_socket Socket connected to the client.
     */
    static void handle_client(SOCKET client_socket);
};

#endif // WEBSOCKETSERVER_HPP
