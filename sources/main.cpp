#include <iostream>
#include "server.h"


int main(int argc, char* argv[]) {
    try {
        Server server(8080);
        server.loadKeys();
        server.startServer();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}