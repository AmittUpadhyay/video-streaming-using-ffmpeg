#include <iostream>
#include <set>
#include <chrono>
#include <thread>
#include <functional> // For std::bind
#include<CVideoStreamSocket.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

VideoStreamSocket::VideoStreamSocket() {
    m_server.init_asio();
    m_server.set_open_handler([this](websocketpp::connection_hdl hdl) { on_open(hdl); });
    m_server.set_close_handler([this](websocketpp::connection_hdl hdl) { on_close(hdl); });
    m_server.set_message_handler([this](websocketpp::connection_hdl hdl, std::shared_ptr<websocketpp::config::core::message_type> msg) { on_message(hdl, msg); });
}

void VideoStreamSocket::run(uint16_t port) {
    m_server.set_http_handler([this](websocketpp::connection_hdl hdl) {
        server::connection_ptr con = m_server.get_con_from_hdl(hdl);
        con->append_header("Access-Control-Allow-Origin", "*");
        con->append_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE");
        con->append_header("Access-Control-Allow-Headers", "Content-Type");
        con->set_body("WebSocket server ready");
        con->set_status(websocketpp::http::status_code::ok);
    });

    m_server.listen(port);
    m_server.start_accept();
    m_server.run();
}

void VideoStreamSocket::on_open(websocketpp::connection_hdl hdl) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.insert(hdl);
        m_client_connected = true;
    }
    m_cv.notify_all();
}

void VideoStreamSocket::on_close(websocketpp::connection_hdl hdl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connections.erase(hdl);
    if (m_connections.empty()) {
        m_client_connected = false;
    }
}

void VideoStreamSocket::on_message(websocketpp::connection_hdl hdl, server::message_ptr msg) {
    std::string payload = msg->get_payload();
    std::cout << "Received message: " << payload << std::endl;

    if (payload == "get_epoch") {
        auto now = std::chrono::system_clock::now();
        auto epoch = std::chrono::system_clock::to_time_t(now);
        std::string epoch_str = std::to_string(epoch);
        m_server.send(hdl, epoch_str, websocketpp::frame::opcode::text);
        std::cout << "Sent epoch time: " << epoch_str << std::endl;
    } else {
        std::cout << "Unknown command received" << std::endl;
    }
}

void VideoStreamSocket::send_video_data(const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& hdl : m_connections) {
        m_server.send(hdl, data.data(), data.size(), websocketpp::frame::opcode::binary);
        std::cout << "Sent data of size: " << data.size() << std::endl;
    }
}
