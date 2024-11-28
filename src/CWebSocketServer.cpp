#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <CWebSocketServer.hpp>

const char* env_p = std::getenv("WEBSOCKET_MAGIC_STRING");
const std::string WebSocketServer::WEBSOCKET_MAGIC_STRING = env_p;

WebSocketServer::WebSocketServer(int port) : port(port), listen_socket(INVALID_SOCKET) {}

WebSocketServer::~WebSocketServer() {
    if (listen_socket != INVALID_SOCKET) {
        closesocket(listen_socket);
        WSACleanup();
    }


}



std::string WebSocketServer::base64_encode(const unsigned char* input, int length) {
    BIO* bmem = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    std::string output(bptr->data, bptr->length - 1);
    BIO_free_all(b64);
    return output;
}

std::string WebSocketServer::sha1_hash(const std::string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(), hash);
    return std::string(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);
}

void WebSocketServer::send_websocket_frame(SOCKET client_socket, const std::vector<unsigned char>& data) {
    std::vector<unsigned char> frame;
    frame.push_back(0x82); // FIN bit set and binary frame

    if (data.size() <= 125) {
        frame.push_back(static_cast<unsigned char>(data.size()));
    } else if (data.size() <= 65535) {
        frame.push_back(126);
        frame.push_back((data.size() >> 8) & 0xFF);
        frame.push_back(data.size() & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back((data.size() >> (i * 8)) & 0xFF);
        }
    }

    frame.insert(frame.end(), data.begin(), data.end());
    send(client_socket, reinterpret_cast<const char*>(frame.data()), frame.size(), 0);
}

void WebSocketServer::send_fmp4_data(SOCKET client_socket, const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << file_path << std::endl;
        return;
    }

    std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    send_websocket_frame(client_socket, buffer);
}

void WebSocketServer::handle_client(SOCKET client_socket) {
    char buffer[1024]{'\0'};
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    std::string request(buffer, bytes_received);
    std::string key{""};
    std::istringstream request_stream(request);
    std::string line{""};
    while (std::getline(request_stream, line) && line != "\r") {
        if (line.find("Sec-WebSocket-Key:") != std::string::npos) {
            key = line.substr(line.find(":") + 2);
            key = key.substr(0, key.size() - 1); // Remove trailing \r
        }
    }

    std::string accept_key = base64_encode(reinterpret_cast<const unsigned char*>(sha1_hash(key + WEBSOCKET_MAGIC_STRING).c_str()), SHA_DIGEST_LENGTH);

    std::ostringstream response{""};
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";

    send(client_socket, response.str().c_str(), response.str().size(), 0);

    // Send FMP4 data
    send_fmp4_data(client_socket, "./output/muxed_output.fmp4");

    closesocket(client_socket);
}

void WebSocketServer::start() {
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return;
    }
    std::cout << "WSAStartup succeeded." << std::endl;

    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }
    std::cout << "Socket created successfully." << std::endl;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    result = bind(listen_socket, (sockaddr*)&server_addr, sizeof(server_addr));
    if (result == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_socket);
        WSACleanup();
        return;
    }
    std::cout << "Socket bound to port " << port << "." << std::endl;

    result = listen(listen_socket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_socket);
        WSACleanup();
        return;
    }
    std::cout << "Listening on port " << port << "..." << std::endl;

    while (true) {
        SOCKET client_socket = accept(listen_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            closesocket(listen_socket);
            WSACleanup();
            return;
        }
        std::cout << "Accepted a connection..." << std::endl;
        std::thread(handle_client, client_socket).detach();
    }
}


