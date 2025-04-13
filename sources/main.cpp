#include <iostream>
#include "server.h"

std::string getLocalIP() {
    asio::io_context io_context;
    asio::ip::tcp::resolver resolver(io_context);
    asio::ip::tcp::resolver::results_type endpoints =
        resolver.resolve(asio::ip::host_name(), "");

    for (const auto& endpoint : endpoints) {
        auto addr = endpoint.endpoint().address();
        if (addr.is_v4() && !addr.is_loopback()) {
            return addr.to_string();
        }
    }
    return ""; 
}

std::string getPublicFacingIP() {
    try {
        asio::io_context io_context;
        asio::ip::tcp::socket socket(io_context);
        
        socket.connect(asio::ip::tcp::endpoint(
            asio::ip::make_address("8.8.8.8"), 53));
        return socket.local_endpoint().address().to_string();
    }
    catch (...) {
        return "";
    }
}

std::string getServerIP() {
    std::string ip = getPublicFacingIP();
    if (!ip.empty()) {
        return ip;
    }

    ip = getLocalIP();
    if (!ip.empty()) {
        return ip;
    }

    return "127.0.0.1";
}

int main() {
    try {
        Server server;
        std::string ip = getServerIP();
        std::cout << "Server IP: " << ip << std::endl;
        server.init(ip, 8080);
        server.run();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}