#include <iostream>
#include <thread>
#include <asio.hpp>
#include "server.h"

int main() {
    try {
        Server server;
        server.init("192.168.1.49", 8080);
        server.run();
    }
    catch (std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
    }

    return 0;
}