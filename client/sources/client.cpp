#include "Client.h"
#include <iostream>

Client::Client() : m_socket(m_io_context), m_isReceiving(true),
sh_is_authorized(OperationResult::NOT_STATED),
sh_is_message_send(OperationResult::NOT_STATED),
sh_is_info_updated(OperationResult::NOT_STATED),
sh_is_message_read_confirmation_send(OperationResult::NOT_STATED),
sh_chat_create(OperationResult::NOT_STATED),
sh_is_status_send(OperationResult::NOT_STATED),
m_my_photo(Photo()), m_is_has_photo(false),
m_worker(nullptr) {}

void Client::init() {
    m_receiverThread = std::thread([this]() { receive(); });
}

void Client::connectTo(const std::string& ipAddress, int port) {
    try {
        asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ipAddress), port);
        m_socket.connect(endpoint); 
        std::cout << "Подключение успешно!" << std::endl;
    }
    catch (const asio::system_error& e) {
        std::cerr << "Ошибка подключения: " << e.what() << " (код ошибки: " << e.code() << ")" << std::endl;
    }
}

void Client::close() {
    m_isReceiving = false;
    if (m_receiverThread.joinable()) {
        m_receiverThread.join();
    }
    m_socket.close();
}

void Client::receive() {
    try {
        std::string accumulated_data;
        while (m_isReceiving) {
            std::vector<char> buffer(1024);
            std::error_code error;

            // Читаем данные до разделителя
            size_t length = asio::read_until(m_socket, asio::dynamic_buffer(accumulated_data), "_+14?bb5HmR;%@`7[S^?!#sL8", error);

            if (error == asio::error::eof) {
                std::cout << "Connection closed by server." << std::endl;
                break;
            }
            else if (error) {
                throw asio::system_error(error);
            }

            size_t pos = accumulated_data.find("_+14?bb5HmR;%@`7[S^?!#sL8");
            while (pos != std::string::npos) {
                std::string packet = accumulated_data.substr(0, pos);
                std::cout << "Received: " << packet << std::endl;
                handleResponse(packet);

                accumulated_data.erase(0, pos + std::string("_+14?bb5HmR;%@`7[S^?!#sL8").length());
                pos = accumulated_data.find("_+14?bb5HmR;%@`7[S^?!#sL8"); 
            }
        }
    }
    catch (std::exception& e) {
        std::cerr << "Receive error: " << e.what() << std::endl;
    }
}

void Client::handleResponse(const std::string& packet) {
    std::istringstream iss(packet);

    std::string type;
    std::getline(iss, type);
    
    if (type == "REGISTRATION_SUCCESS") {
        sh_is_authorized = OperationResult::SUCCESS;
    }
    else if (type == "REGISTRATION_FAIL") {
        sh_is_authorized = OperationResult::FAIL;
    }
    else if (type == "AUTHORIZATION_SUCCESS") {
        sh_is_authorized = OperationResult::SUCCESS;
    }
    else if (type == "AUTHORIZATION_FAIL") {
        sh_is_authorized = OperationResult::FAIL;
    }
    else if (type == "CHAT_CREATE_SUCCESS") {
        sh_chat_create = OperationResult::SUCCESS;
    }
    else if (type == "CHAT_CREATE_FAIL") {
        sh_chat_create = OperationResult::FAIL;
    }
    else if (type == "MESSAGE_SUCCESS") {
        sh_is_message_send = OperationResult::SUCCESS;
    }
    else if (type == "MESSAGE_FAIL") {
        sh_is_message_send = OperationResult::FAIL;
    }
    else if (type == "MESSAGE_READ_CONFIRAMTION_SUCESS") {
        sh_is_message_read_confirmation_send = OperationResult::SUCCESS;
    }
    else if (type == "MESSAGE_READ_CONFIRAMTION_FAIL") {
        sh_is_message_read_confirmation_send = OperationResult::FAIL;
    }


    else if (type == "STATUS") {
        m_worker->onStatusReceive();
    }
    else if (type == "MESSAGE") {
        m_worker->onMessageReceive();
    }
    else if (type == "MESSAGE_READ_CONFIRMATION") {
        m_worker->onMessageReadConfirmationReceive();
    }
}

OperationResult Client::authorizeClient(const std::string& login, const std::string& password) {
    sendPacket(m_sender.get_authorization_QueryStr(login, password));

    auto startTime = std::chrono::steady_clock::now(); 
    auto timeout = std::chrono::seconds(5); 

    while (sh_is_authorized == OperationResult::NOT_STATED) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime);

        if (elapsedTime >= timeout) {
            return OperationResult::REQUEST_TIMEOUT;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (sh_is_authorized == OperationResult::SUCCESS) {
        return OperationResult::SUCCESS;
    }
    else {
        return OperationResult::FAIL;
    }
}

OperationResult Client::registerClient(const std::string& login, const std::string& password, const std::string& name) {
    m_my_login = login;
    m_my_name = name;
    m_my_password = password;
    sendPacket(m_sender.get_registration_QueryStr(login, name, password));

    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(5);

    while (sh_is_authorized == OperationResult::NOT_STATED) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime);

        if (elapsedTime >= timeout) {
            return OperationResult::REQUEST_TIMEOUT;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (sh_is_authorized == OperationResult::SUCCESS) {
        return OperationResult::SUCCESS;
    }
    else {
        return OperationResult::FAIL;
    }
}

OperationResult Client::createChatWith(const std::string& friendLogin) {
    std::lock_guard<std::mutex> guard(m_mtx);

    m_map_friend_login_to_chat[friendLogin] = new Chat;
    sendPacket(m_sender.get_createChat_QueryStr(m_my_login, friendLogin));

    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(5);

    while (sh_chat_create == OperationResult::NOT_STATED) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime);

        if (elapsedTime >= timeout) {
            return OperationResult::REQUEST_TIMEOUT;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (sh_chat_create == OperationResult::SUCCESS) {
        return OperationResult::SUCCESS;
    }

    else {
        return OperationResult::FAIL;
    }

}

OperationResult Client::sendMyStatus(const std::string& status) {
    std::vector<std::string> friendsLoginsVec;
    for (const auto& pair : m_map_friend_login_to_chat) {
        friendsLoginsVec.push_back(pair.first);
    }
    std::string packet = m_sender.get_status_ReplyStr(status, m_my_login, friendsLoginsVec);
    sendPacket(packet);

    return waitForResponse(5, [this] {
        return sh_is_status_send;
        });
}

OperationResult Client::updateMyInfo(const std::string& login, const std::string& name, const std::string& password, bool isHasPhoto, Photo photo) {
    m_my_login = login;
    m_my_name = name;
    m_my_password = password;
    m_is_has_photo = isHasPhoto;
    m_my_photo = photo;
    std::string packet = m_sender.get_updateMyInfo_QueryStr(login, name, password, isHasPhoto, photo);
    sendPacket(packet);

    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(5);

    while (sh_is_info_updated == OperationResult::NOT_STATED) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime);

        if (elapsedTime >= timeout) {
            return OperationResult::REQUEST_TIMEOUT;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (sh_is_info_updated == OperationResult::SUCCESS) {
        return OperationResult::SUCCESS;
    }
    else {
        return OperationResult::FAIL;
    }
}

OperationResult Client::waitForResponse(int seconds, std::function<OperationResult()> checkFunction) {
    auto startTime = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(seconds); 
    while (true) { 
        if (checkFunction() != OperationResult::FAIL) {
            return checkFunction();
        }
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime);
        if (elapsedTime >= timeout) {
            return OperationResult::FAIL;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    sh_is_message_send = OperationResult::NOT_STATED;
}

OperationResult Client::sendMessage(const std::string& friendLogin, const std::string& message, std::string timestamp) {
    std::lock_guard<std::mutex> guard(m_mtx);
    sendPacket(m_sender.get_message_ReplyStr(m_my_login, friendLogin, message, timestamp));
    return waitForResponse(5, [this] {
        return sh_is_message_send;
        });
}

OperationResult Client::sendMessageReadConfirmation(const std::string& friendLogin, const std::vector<int>& messagesReadIdsVec) {
    std::lock_guard<std::mutex> guard(m_mtx);
    sendPacket(m_sender.get_messageReadConfirmation_ReplyStr(m_my_login, friendLogin, messagesReadIdsVec));
    return waitForResponse(5, [this] {
        return sh_is_message_read_confirmation_send;
        });
}

void Client::setWorkerUI(WorkerUI* workerImpl) {
    m_worker = workerImpl;
}

void Client::sendPacket(const std::string& packet) {
    asio::write(m_socket, asio::buffer(packet), asio::transfer_all());
}
