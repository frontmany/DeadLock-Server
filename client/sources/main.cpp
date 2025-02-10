#include <iostream>
#include <asio.hpp>

#include "client.h"

using asio::ip::tcp;

class MyWorkerUI : public WorkerUI {
public:
    void onStatusReceive() override {
        std::cout << "Статус получен!" << std::endl;
    }

    void onMessageReceive() override {
        std::cout << "Сообщение получено!" << std::endl;
    }

    void onMessageReadConfirmationReceive() override {
        std::cout << "Подтверждение прочтения сообщения получено!" << std::endl;
    }
};

int main() {
    setlocale(LC_ALL, "ru");

    Client client;
    MyWorkerUI workerUI;

    client.init();
    client.setWorkerUI(&workerUI);
    client.connectTo("127.0.0.1", 8080);

    OperationResult authResult = client.registerClient("user", "password", "ddaa");
    if (authResult == OperationResult::SUCCESS) {
        std::cout << "Registration success!" << std::endl;

        
        client.sendMessage("user", "what's up?", "12.02");
    }
    else {
        std::cout << "Registration error." << std::endl;
    }

    client.close();
    return 0;
}
