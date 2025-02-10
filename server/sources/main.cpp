#include <iostream>
#include <thread>
#include <asio.hpp>
#include "Server.h"

int main() {
    try {
        Server server;
        server.init("127.0.0.1", 8080);
        server.run();
    }
    catch (std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
    }

    return 0;
}