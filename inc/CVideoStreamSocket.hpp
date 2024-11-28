#ifndef VIDEOSTREAMSOCKET_HPP
#define VIDEOSTREAMSOCKET_HPP

#include <set>
#include <vector>
#include <mutex>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

/**
 * @class VideoStreamSocket
 * @brief Class for handling video streaming over WebSocket.
 *
 * This class manages WebSocket connections and handles the transmission of video data to connected clients.
 */
class VideoStreamSocket {
public:

    /**
     * @brief Constructor for VideoStreamSocket.
     */
    VideoStreamSocket();

    /**
     * @brief Run the WebSocket server on the specified port.
     *
     * This method starts the WebSocket server and listens for incoming connections on the given port.
     *
     * @param port The port number to run the server on.
     */
    void run(uint16_t port);

    /**
     * @brief Send video data to all connected clients.
     *
     * This method sends the provided video data to all clients currently connected to the WebSocket server.
     *
     * @param data A vector containing the video data to be sent.
     */
    void send_video_data(const std::vector<uint8_t>& data);
    bool m_client_connected = false; ///< boolean to check if client is connected already
    std::mutex m_mutex;
    std::condition_variable m_cv;

private:
    /**
     * @brief Handle a new WebSocket connection.
     *
     * This method is called when a new client connects to the WebSocket server.
     *
     * @param hdl The handle for the new connection.
     */
    void on_open(websocketpp::connection_hdl hdl);

    /**
     * @brief Handle a WebSocket connection closure.
     *
     * This method is called when a client disconnects from the WebSocket server.
     *
     * @param hdl The handle for the closed connection.
     */
    void on_close(websocketpp::connection_hdl hdl);

    /**
     * @brief Handle incoming messages from clients.
     *
     * This method is called when a message is received from a client.
     *
     * @param hdl The handle for the connection that sent the message.
     * @param msg The message received from the client.
     */
    void on_message(websocketpp::connection_hdl hdl, server::message_ptr msg);
    

    server m_server; ///< The WebSocket server instance.
    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> m_connections; ///< Set of active connections.


};

#endif // VIDEOSTREAMSOCKET_HPP