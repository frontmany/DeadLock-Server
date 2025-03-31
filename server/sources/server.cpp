#include "Server.h"
#include "hasher.h"
#include "base64.h"
#include <iostream>
#include <algorithm>
#include <codecvt>
#include <locale>

Server::Server()
    : m_acceptor(m_io_context),
    m_thread_pool(std::thread::hardware_concurrency()), m_port(-1), m_db(Database()) {}

void Server::init(const std::string& ipAddress, int port) {
    m_db.init();
    m_port = port;
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ipAddress), m_port);
    m_acceptor.open(endpoint.protocol());
    m_acceptor.set_option(asio::socket_base::reuse_address(true));
    m_acceptor.bind(endpoint);
    m_acceptor.listen();
}

void Server::run() {
    acceptConnections();
    for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
        m_vec_threads.emplace_back([this]() { m_io_context.run(); });
    }
    for (auto& thread : m_vec_threads) {
        thread.join();
    }
}

void Server::acceptConnections() {
    auto socket = new asio::ip::tcp::socket(m_io_context);
    m_acceptor.async_accept(*socket, [this, socket](std::error_code ec) {
        if (!ec) {
            onAccept(socket);
        }
        acceptConnections(); // Continue accepting new connections
        });
}

void Server::startAsyncRead(asio::ip::tcp::socket* socket) {
    auto buffer = std::make_shared<asio::streambuf>();
    buffer->prepare(1024 * 1024 * 20);
    asio::async_read_until(*socket, *buffer, "_+14?bb5HmR;%@`7[S^?!#sL8",
        [this, socket, buffer](const asio::error_code& ec, std::size_t bytes_transferred) {
            handleRead(ec, bytes_transferred, socket, buffer);
        });
}

void Server::onAccept(asio::ip::tcp::socket* socket) {
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        std::cout << "client connected on socket:" <<  socket->remote_endpoint() << '\n';
    }
    startAsyncRead(socket);
}

void Server::onDisconnect(asio::ip::tcp::socket* socket) {
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = std::find_if(m_map_online_users.begin(), m_map_online_users.end(), [socket](const auto& pair) {
        return pair.second->getSocketOnServer() == socket;
        });

    if (it != m_map_online_users.end()) {
        std::cout << "client disconnected on socket:" << socket->remote_endpoint() << '\n';

        User* user = it->second;

        for (auto pair : m_vec_login_to_login) {
            if (pair.first == user->getLogin()) {
                auto it2 = m_map_online_users.find(pair.second);
                if (it2 != m_map_online_users.end()) {
                    User* userTo = it2->second;
                    sendResponse(userTo->getSocketOnServer(), m_sender.get_statusStr(user->getLogin(), m_db.getCurrentDateTime()));
                }
            }
        }

        m_db.updateUserStatus(user->getLogin(), m_db.getCurrentDateTime());
        m_map_online_users.erase(it);
        delete user;
    }
    else {
        std::cout << "client disconnected on socket:" << socket->remote_endpoint() << '\n';
        socket->close();
        delete socket;
    }
}


void Server::handleRead(const asio::error_code& ec, std::size_t bytes_transferred,
    asio::ip::tcp::socket* socket, std::shared_ptr<asio::streambuf> buffer) {

    if (ec == asio::error::eof || ec == asio::error::connection_reset) {
        onDisconnect(socket);
        return;
    }

    if (ec) {
        std::cerr << "Read error: " << ec.message() << std::endl;
        return; 
        
    }

    std::istream is(buffer.get());
    std::string packet((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    std::istringstream iss(packet);
    std::string classificationStr;
    std::getline(iss, classificationStr);

    std::string remainingStr = rebuildRemainingStringFromIss(iss);
    if (classificationStr == "GET") {
        handleGet(socket, remainingStr);
    }
    else if (classificationStr == "RPL") {
        handleRpl(socket, remainingStr);
    }
    else if (classificationStr == "BROADCAST") {
        handleBroadcast(socket, remainingStr);
    }

    startAsyncRead(socket);
}

void Server::handleBroadcast(asio::ip::tcp::socket* socket, std::string packet) {
    std::istringstream iss(packet);

    std::string typeStr;
    std::getline(iss, typeStr);

    std::string remainingStr = rebuildRemainingStringFromIss(iss);
    if (typeStr == "STATUS") {
        broadcastUserInfo(socket, remainingStr);
    }
}

void Server::broadcastUserInfo(asio::ip::tcp::socket* socket, std::string packet) {
    std::istringstream iss(packet);

    std::string status;
    std::getline(iss, status);

    std::string login;
    std::getline(iss, login);

    std::string line;
    while (std::getline(iss, line)) {
        if (line == "VEC_BEGIN") {
            continue;
        }
        if (line == "VEC_END") {
            return;
        }
        else {
            auto it = m_map_online_users.find(line);
            if (it == m_map_online_users.end()) {
                continue; 
            }
            else {
                sendResponse(it->second->getSocketOnServer(), m_sender.get_statusStr(login, status));
            }
        }
    }
}

void Server::handleGet(asio::ip::tcp::socket* socket, std::string packet) {
    std::istringstream iss(packet);

    std::string typeStr;
    std::getline(iss, typeStr);

    std::string remainingStr = rebuildRemainingStringFromIss(iss);
    if (typeStr == "AUTHORIZATION") {
        authorizeUser(socket, remainingStr);
    }
    else if (typeStr == "REGISTRATION") {
        registerUser(socket, remainingStr);
    }
    else if (typeStr == "CREATE_CHAT") {
        createChat(socket, remainingStr);
    }
    else if (typeStr == "UPDATE_MY_INFO") {
        updateUserInfo(socket, remainingStr);
    }
    else if (typeStr == "LOAD_FRIEND_INFO") {
        returnUserInfo(socket, remainingStr);
    }
    else if (typeStr == "LOAD_ALL_FRIENDS_STATUSES") {
        findFriendsStatuses(socket, remainingStr);
    }
    else if (typeStr == "CHECK_NEW_LOGIN") {
        checkIsNewLoginAvailable(socket, remainingStr);
    }
}

void Server::handleRpl(asio::ip::tcp::socket* socket, std::string packet) {
    std::istringstream iss(packet);

    std::string friendLogin;
    std::getline(iss, friendLogin);

    std::string type;
    std::getline(iss, type);

    std::string myLogin;
    std::getline(iss, myLogin);

    auto it = m_map_online_users.find(friendLogin); 

    if (it == m_map_online_users.end()) {
        if (type == "MESSAGE") {
            m_db.collect(friendLogin, "MESSAGE\n" + iss.str());
            sendResponse(socket, m_sender.get_messageSuccessStr());
        }
        else if (type == "MESSAGES_READ_CONFIRMATION") {
            m_db.collect(friendLogin, "MESSAGES_READ_CONFIRMATION\n" + iss.str());
            sendResponse(socket, m_sender.get_messageReadConfirmationSuccessStr());
        }
        else if (type == "FIRST_MESSAGE") {
            m_db.collect(friendLogin, "FIRST_MESSAGE\n" + iss.str());
            sendResponse(socket, m_sender.get_messageSuccessStr());
        }
    }
    else {
        User* user = it->second;

        if (type == "MESSAGE") {
            sendResponse(user->getSocketOnServer(), "MESSAGE\n" + iss.str());
            sendResponse(socket, m_sender.get_messageSuccessStr());
        }
        else if (type == "FIRST_MESSAGE") {
            auto itPair = std::find_if(m_vec_login_to_login.begin(), m_vec_login_to_login.end(), [&myLogin, &friendLogin](std::pair<std::string, std::string> pair) {
                return pair.first == myLogin && pair.second == friendLogin;
                });

            if (itPair != m_vec_login_to_login.end()) {
                m_vec_login_to_login.erase(itPair);
            }

            sendResponse(user->getSocketOnServer(), "FIRST_MESSAGE\n" + iss.str());
            sendResponse(socket, m_sender.get_messageSuccessStr());
        }
        else if (type == "MESSAGES_READ_CONFIRMATION") {
            sendResponse(user->getSocketOnServer(), "MESSAGES_READ_CONFIRMATION\n" + iss.str());
            sendResponse(socket, m_sender.get_messageReadConfirmationSuccessStr());
        }
    }
}

std::string Server::rebuildRemainingStringFromIss(std::istringstream& iss) {
    std::string remainingStr;
    std::string line;
    while (std::getline(iss, line)) {
        remainingStr += line + '\n';
    }
    remainingStr.pop_back();
    return remainingStr;
}

void Server::checkIsNewLoginAvailable(asio::ip::tcp::socket* socket, std::string packet) {
    std::lock_guard<std::mutex> lock(m_mtx);

    std::istringstream iss(packet);
    std::string newLogin;
    std::getline(iss, newLogin);
    bool isAvailable = m_db.checkIsNewLoginAvailable(newLogin);

    std::string packetResponse;
    if (isAvailable) {
        packetResponse = m_sender.get_newLoginSuccessStr();
    }
    else {
        packetResponse = m_sender.get_newLoginFailStr();
    }

    sendResponse(socket, packetResponse);
}

void Server::returnUserInfo(asio::ip::tcp::socket* socket, std::string packet) {
    std::lock_guard<std::mutex> lock(m_mtx);

    std::istringstream iss(packet);
    std::string login;
    std::getline(iss, login);

    User* user = m_db.getUser(login, socket);
    if (user == nullptr) {
        std::string response = m_sender.get_userInfoFailStr();
        sendResponse(socket, response);
    }
    else {
        std::string response;
        auto it = m_map_online_users.find(user->getLogin());
        if (it == m_map_online_users.end()) {
            response = m_sender.get_userInfoSuccessStr(user);
        }
        else {
            response = m_sender.get_userInfoSuccessStr(it->second);
        }
        sendResponse(socket, response);
    }
}

void Server::findFriendsStatuses(asio::ip::tcp::socket* socket, std::string packet) {
    std::lock_guard<std::mutex> lock(m_mtx);
    std::istringstream iss(packet);
    std::string vecBegin;
    std::getline(iss, vecBegin);

    std::vector<std::string> logins;
    std::vector<std::string> statuses;

    std::string line;
    while (std::getline(iss, line)) {
        if (line == "VEC_END") {
            break;
        }
        else {
            User* user = m_db.getUser(line, socket);
            if (user != nullptr) {
                auto it = m_map_online_users.find(user->getLogin());
                logins.push_back(user->getLogin());
                if (it == m_map_online_users.end()) {
                    statuses.push_back(user->getLastSeen());
                }
                else {
                    statuses.push_back("online");
                }
            }
            else {
                statuses.push_back("recently");
            }
        }
    }
    sendResponse(socket, m_sender.get_friendsStatusesSuccessStr(logins, statuses));

    auto it = std::find_if(m_map_online_users.begin(), m_map_online_users.end(), [socket](const auto& pair) {
        return pair.second->getSocketOnServer() == socket;
        });

    if (it != m_map_online_users.end()) {
        User* user = it->second;

        auto packets = m_db.getCollected(user->getLogin());
        for (int i = 0; i < packets.size(); i++){
            sendResponse(user->getSocketOnServer(), packets[i]);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        
    }
}

void Server::authorizeUser(asio::ip::tcp::socket* socket, std::string packet) {
    std::lock_guard<std::mutex> lock(m_mtx);

    std::istringstream iss(packet);

    std::string login;
    std::getline(iss, login);

    std::string password;
    std::getline(iss, password);

    if (m_map_online_users.find(login) != m_map_online_users.end()) {
        std::string response = m_sender.get_authorizationFailStr();
        sendResponse(socket, response);
        return;
    }

    bool isAuthorized = m_db.checkPassword(login, password);
    if (isAuthorized) {
        User* user = m_db.getUser(login, socket);

        if (user == nullptr) {
            std::cout << "can't get user info";
        }

        else {
            user->setLastSeenToOnline();
            m_map_online_users[login] = user;

            std::string response = m_sender.get_authorizationSuccessStr();
            sendResponse(user->getSocketOnServer(), response);

            for (auto pair : m_vec_login_to_login) {
                if (pair.first == login) {
                    auto it = m_map_online_users.find(pair.second);
                    if (it != m_map_online_users.end()) {
                        User* userTo = it->second;
                        sendResponse(userTo->getSocketOnServer(), m_sender.get_statusStr(user->getLogin(), "online"));
                    }
                }
            }
        }
    }
    else {
        std::string response = m_sender.get_authorizationFailStr();
        sendResponse(socket, response);
    }
}


bool is_valid_utf8(const std::string& str) {
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wide = converter.from_bytes(str);
        (void)wide; // Убеждаемся, что конвертация прошла без ошибок
        return true;
    }
    catch (const std::range_error&) {
        return false;
    }
}

#ifdef _WIN32
std::string convert_to_utf8(const std::string& str) {
    try {
        if (is_valid_utf8(str)) {
            return str; // Уже UTF-8
        }
        // Предполагаем, что строка в Windows-1251 (если не UTF-8)
        std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
        std::wstring wide_str = utf8_conv.from_bytes(str);
        return utf8_conv.to_bytes(wide_str);
    }
    catch (...) {
        return str; // Если не удалось конвертировать, возвращаем как есть
    }
}
#else
std::string convert_to_utf8(const std::string& str) {
    return str; // На Linux/macOS обычно уже UTF-8
}
#endif

void Server::registerUser(asio::ip::tcp::socket* socket, std::string packet) {
    std::lock_guard<std::mutex> lock(m_mtx);

    std::istringstream iss(packet);

    std::string login;
    std::getline(iss, login);
    login = convert_to_utf8(login);

    std::string name;
    std::getline(iss, name);
    name = convert_to_utf8(name);

    std::string password;
    std::getline(iss, password);
    password = convert_to_utf8(password);

    if (m_db.getUser(login) == nullptr) {
        std::string passwordHash = hash::hashPassword(password);
        User* user = new User(login, passwordHash, name, false, Photo(), socket);
        user->setLastSeenToOnline();
        m_map_online_users[login] = user;

        std::string response = m_sender.get_registrationSuccessStr();
        sendResponse(socket, response);
        m_db.addUser(login, name, user->getLastSeen(), passwordHash);
    }
    else {
        std::string response = m_sender.get_registrationFailStr();
        sendResponse(socket, response);
    }
}

void Server::createChat(asio::ip::tcp::socket* socket, std::string packet) {
    std::lock_guard<std::mutex> lock(m_mtx);

    std::istringstream iss(packet);

    std::string myLogin;
    std::getline(iss, myLogin);

    std::string friendLogin;
    std::getline(iss, friendLogin);

    std::string response;

    if (myLogin == friendLogin) {
        response = m_sender.get_chatCreateFailStr();
    }
    else {
        setlocale(LC_ALL, "ru");

        User* user = m_db.getUser(friendLogin);
        if (user == nullptr) {
            response = m_sender.get_chatCreateFailStr();
        }
        else {
            response = m_sender.get_chatCreateSuccessStr(user);

            auto it = m_map_online_users.find(friendLogin);
            if (it == m_map_online_users.end()) {
                m_vec_login_to_login.push_back(std::make_pair(friendLogin, myLogin));
                std::cerr << "User " << friendLogin << " not found in online users." << std::endl;
            }
            else
            {
                sendResponse(it->second->getSocketOnServer(), m_sender.get_loginToSendStatusStr(myLogin));
                
            }
        }
    }


    sendResponse(socket, response);
}

void Server::updateUserInfo(asio::ip::tcp::socket* socket, std::string packet) {
    std::lock_guard<std::mutex> lock(m_mtx);

    std::istringstream iss(packet);

    std::string oldLogin;
    std::getline(iss, oldLogin);
    oldLogin = convert_to_utf8(oldLogin); // Конвертируем в UTF-8

    std::string newLogin;
    std::getline(iss, newLogin);
    newLogin = convert_to_utf8(newLogin); // Конвертируем в UTF-8

    std::string name;
    std::getline(iss, name);
    name = convert_to_utf8(name); // Конвертируем в UTF-8

    std::string password;
    std::getline(iss, password);
    password = convert_to_utf8(password);

    std::string vecBegin;
    std::getline(iss, vecBegin);

    std::vector<std::string> logins;
    std::string line;
    while (std::getline(iss, line)) {
        if (line == "VEC_END") {
            break;
        }
        else {
            logins.push_back(line);
        }
    }

    std::string isHasPhotoStr;
    std::getline(iss, isHasPhotoStr);
    bool isHasPhoto = isHasPhotoStr == "true";

    std::string photoSizeStr;
    std::getline(iss, photoSizeStr);

    size_t size = 0;
    if (photoSizeStr != "") {
        size = std::stoi(photoSizeStr);
    }

    std::string photoStr;
    std::getline(iss, photoStr);

    for (auto login : logins) {
        auto it = m_map_online_users.find(login);
        if (it != m_map_online_users.end()) {
            User* user = it->second;
            std::string packetU = m_sender.get_userInfoPacket(oldLogin, newLogin, name, isHasPhotoStr, photoSizeStr, photoStr);
            sendResponse(user->getSocketOnServer(), packetU);
        }
        else {
            User* user = m_db.getUser(login);
            std::string packetU = m_sender.get_userInfoPacket(oldLogin, newLogin, name, isHasPhotoStr, photoSizeStr, photoStr);
            m_db.collect(login, packetU);
        }
    }

    if (oldLogin != newLogin) {
        auto it = m_map_online_users.find(oldLogin);
        if (it != m_map_online_users.end()) { 
            User* user = it->second;
            m_map_online_users.erase(it);
            m_map_online_users[newLogin] = user;

            user->setLogin(newLogin); 
        }

        for (auto [login1, login2] : m_vec_login_to_login) {
            if (login1 == oldLogin) {
                login1 = newLogin;
            }
            else if (login2 == oldLogin) {
                login2 = newLogin;
            }
        }
    }

    Photo photo;
    if (isHasPhoto) {
        photo = Photo::deserialize(base64_decode(photoStr), size, oldLogin, newLogin);
    }

    if (isHasPhoto) {
        m_db.updateUser(oldLogin, newLogin, name, password, isHasPhoto, photo);
    }
    else {
        m_db.updateUser(oldLogin, newLogin, name, password, isHasPhoto);
    }

    sendResponse(socket, m_sender.get_userInfoUpdatedSuccessStr());
}

void Server::sendResponse(asio::ip::tcp::socket* socket, std::string response) {

    bool is_lock = m_mtx.try_lock();
    

    asio::async_write(*socket, asio::buffer(response.data(), response.size()),
        [socket, this](const asio::error_code& error, std::size_t bytes_transferred) {
            if (error) {
                std::cerr << "Send error: " << error.message() << std::endl;
                // Handle the error (e.g., close the socket)
            }
            else {
                {
                    std::lock_guard<std::mutex> lock(m_mtx);
                    std::cout << "Sent " << bytes_transferred << " bytes to " << socket->remote_endpoint() << std::endl;

                }
            }
        });

    if (is_lock) {
        m_mtx.unlock();
    }
}
