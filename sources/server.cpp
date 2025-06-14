#include "server.h"
#include "hasher.h"
#include "base64.h"

#include <iostream>
#include <algorithm>
#include <codecvt>
#include <locale>

void Server::processIncomingMessagesQueue() {
    while (true) {
        update();
    }
}

Server::Server(int port) : net::server_interface<QueryType>(port), m_db(Database()) {
    m_port = port;
}

void Server::startServer() {
    m_db.init();
    start();
    processIncomingMessagesQueue();
}

void Server::stopServer() {
    stop();
}

void Server::onClientDisconnect(connectionT connection) {
    auto it = std::find_if(m_map_online_users.begin(), m_map_online_users.end(),
        [connection](const auto& pair) { return pair.second->getConnection() == connection; });

    if (it != m_map_online_users.end()) {
        User* user = it->second;
        m_db.updateUserStatus(user->getLogin(), m_db.getCurrentDateTime());
        m_map_online_users.erase(user->getLogin());
        delete user;
    }
}

bool Server::isConnectionAllowed(connection_variant& connection) {
    return true;
}

void Server::onMessage(connectionT connection, MessageT msg) {
    std::string messageStr;
    msg >> messageStr;
    std::istringstream iss(messageStr);

    std::string classificationStr;
    std::getline(iss, classificationStr);

    std::string remainingStr = rebuildRemainingStringFromIss(iss);
    if (classificationStr == "GET") {
        handleGet(connection, remainingStr, msg.header.type);
    }
    else if (classificationStr == "RPL") {
        handleRpl(connection, remainingStr, msg.header.type);
    }
    else if (classificationStr == "BROADCAST") {
        handleBroadcast(connection, remainingStr, msg.header.type);
    }
}

void Server::handleBroadcast(connectionT connection, const std::string& stringPacket, QueryType type) {
    if (type == QueryType::STATUS) {
        broadcastUserStatus(connection, stringPacket);
    }
}

void Server::broadcastUserStatus(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

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
                net::message<QueryType> msg;
                msg.header.type = QueryType::STATUS;
                std::string messageStr = m_sender.get_statusStr(login, status);
                msg << messageStr;

                sendResponse(it->second->getConnection(), msg);
            }
        }
    }
}

void Server::handleGet(connectionT connection, const std::string& stringPacket, QueryType type) {

    if (type == QueryType::AUTHORIZATION) {
        authorizeUser(connection, stringPacket);
    }
    else if (type == QueryType::REGISTRATION) {
        registerUser(connection, stringPacket);
    }
    else if (type == QueryType::CREATE_CHAT) {
        createChat(connection, stringPacket);
    }

    else if (type == QueryType::UPDATE_MY_NAME) {
        updateUserName(connection, stringPacket);
    }
    else if (type == QueryType::UPDATE_MY_PASSWORD) {
        updateUserPassword(connection, stringPacket);
    }
    else if (type == QueryType::UPDATE_MY_PHOTO) {
        updateUserPhoto(connection, stringPacket);
    }
    else if (type == QueryType::UPDATE_MY_LOGIN) {
        updateUserLogin(connection, stringPacket);
    }
    else if (type == QueryType::LOAD_FRIEND_INFO) {
        returnUserInfo(connection, stringPacket);
    }
    else if (type == QueryType::LOAD_ALL_FRIENDS_STATUSES) {
        findFriendsStatuses(connection, stringPacket);
    }
    else if (type == QueryType::VERIFY_PASSWORD) {
        verifyPassword(connection, stringPacket);
    }
    else if (type == QueryType::CHECK_NEW_LOGIN) {
        checkNewLogin(connection, stringPacket);
    }
    else if (type == QueryType::FIND_USER) {
        findUser(connection, stringPacket);
    }
    else if (type == QueryType::SEND_ME_FILE) {
        onSendMeFile(connection, stringPacket);
    }
    
}

void Server::handleRpl(connectionT connection, const std::string& stringPacket, QueryType type) {
    std::istringstream iss(stringPacket);

    std::string friendLogin;
    std::getline(iss, friendLogin);

    auto it = m_map_online_users.find(friendLogin); 

    if (it == m_map_online_users.end()) {
        if (type == QueryType::MESSAGE) {
            m_db.collect(friendLogin, iss.str(), QueryType::MESSAGE);
        }
        else if (type == QueryType::MESSAGES_READ_CONFIRMATION) {
            m_db.collect(friendLogin, iss.str(), QueryType::MESSAGES_READ_CONFIRMATION);
        }
    }
    else {
        User* user = it->second;

        if (type == QueryType::MESSAGE) {
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::MESSAGE;
            msgResponse << iss.str();
            sendResponse(user->getConnection(), msgResponse);
        }
        else if (type == QueryType::MESSAGES_READ_CONFIRMATION) {
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::MESSAGES_READ_CONFIRMATION;
            msgResponse << iss.str();
            sendResponse(user->getConnection(), msgResponse);
        }
        else if (type == QueryType::TYPING) {
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::TYPING;
            msgResponse << iss.str();
            sendResponse(user->getConnection(), msgResponse);
        }
    }
}

void Server::onSendMeFile(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string myLogin;
    std::getline(iss, myLogin);

    std::string friendLogin;
    std::getline(iss, friendLogin);

    std::string fileName;
    std::getline(iss, fileName);

    std::string fileId;
    std::getline(iss, fileId);

    std::string fileSize;
    std::getline(iss, fileSize);

    std::string fileTimestamp;
    std::getline(iss, fileTimestamp);


    std::string messageBegin;
    std::getline(iss, messageBegin);

    std::string caption;
    std::string line;
    while (std::getline(iss, line)) {
        if (line == "MESSAGE_END") {
            break;
        }
        else {
            caption += line;
            caption += '\n';
        }
    }
    caption.pop_back();

    std::string filesCountInBlobStr;
    std::getline(iss, filesCountInBlobStr);
    size_t filesCountInBlob = std::stoi(filesCountInBlobStr);

    std::string blobUID;
    std::getline(iss, blobUID);


    const std::string filePath = "ReceivedFiles/" + fileId;
    size_t dotPos = fileName.find_last_of('.');

    std::string fullPath;
    if (dotPos != std::string::npos && dotPos + 1 < fileName.length()) {
        std::string extension = fileName.substr(dotPos + 1);
        fullPath = filePath + "." + extension;
    }
    std::ifstream fileStream(fullPath);
    bool isPresent = fileStream.good();

    if (isPresent) {
        net::file<QueryType> file;
        file.blobUID = blobUID;
        file.caption = caption;
        file.filePath = fullPath;
        file.fileName = fileName;
        file.filesInBlobCount = filesCountInBlob;
        file.fileSize = std::stoi(fileSize);
        file.id = fileId;
        file.receiverLogin =  myLogin;
        file.senderLogin = friendLogin;
        file.timestamp = fileTimestamp;
        file.isRequested = true;

        auto user = m_map_online_users.at(myLogin);
        sendFile(user->getFilesConnection(), file);
    }
    else {
        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::UNEXISTING_FILE;
        msgResponse << stringPacket;
        sendResponse(connection, msgResponse);
    }
}

void Server::onFile(net::file<QueryType> file) {
    auto it = m_map_online_users.find(file.receiverLogin);

    if (it == m_map_online_users.end()) {
        std::string filePreviewStr = m_sender.get_filePreviewStr(file.senderLogin, file.receiverLogin, file.fileName, file.id, std::to_string(file.fileSize), file.timestamp, file.caption, file.blobUID, file.filesInBlobCount);
        m_db.collect(file.receiverLogin, filePreviewStr, QueryType::FILE_PREVIEW);
    }
    else {
        if (file.fileSize > 100000000) { // 100mb
            std::string filePreviewStr = m_sender.get_filePreviewStr(file.senderLogin, file.receiverLogin, file.fileName, file.id, std::to_string(file.fileSize), file.timestamp, file.caption, file.blobUID, file.filesInBlobCount);
            User* user = it->second;
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::FILE_PREVIEW;
            msgResponse << filePreviewStr;
            sendResponse(user->getConnection(), msgResponse);
        }
        else {
            User* user = it->second;
            sendFile(user->getFilesConnection(), file);
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

void Server::findUser(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string myLogin;
    std::getline(iss, myLogin);

    std::string text;
    std::getline(iss, text);

    std::vector<User*> vec;
    m_db.findUsers(myLogin, text, vec);

    net::message<QueryType> msgResponse;
    msgResponse.header.type = QueryType::FIND_USER_RESULTS;
    std::string s = m_sender.get_usersStr(vec);
    msgResponse << s;
    sendResponse(connection, msgResponse);
}

void Server::checkNewLogin(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string login;
    std::getline(iss, login);


    bool isAvailable = m_db.checkNewLogin(login);

    net::message<QueryType> msgResponse;
    if (isAvailable) {
        msgResponse.header.type = QueryType::NEW_LOGIN_SUCCESS;
        msgResponse << login;
    }
    else
        msgResponse.header.type = QueryType::NEW_LOGIN_FAIL;

    sendResponse(connection, msgResponse);
}

void Server::verifyPassword(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string login;
    std::getline(iss, login);

    std::string passwordHash;
    std::getline(iss, passwordHash);

    bool isPasswordsMatch = m_db.checkPassword(login, passwordHash);

    net::message<QueryType> msgResponse;
    if (isPasswordsMatch) 
        msgResponse.header.type = QueryType::VERIFY_PASSWORD_SUCCESS;
    else
        msgResponse.header.type = QueryType::VERIFY_PASSWORD_FAIL;

    sendResponse(connection, msgResponse);
}

void Server::returnUserInfo(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);
    std::string login;
    std::getline(iss, login);

    std::string response;
    auto it = m_map_online_users.find(login);

    if (it == m_map_online_users.end()) {
        User* user = m_db.getUser(login);
        response = m_sender.get_userInfoPacket(user);
        delete user;
    }
    else {
        User* user = it->second;
        response = m_sender.get_userInfoPacket(user);
    }

    net::message<QueryType> msgResponse;
    msgResponse.header.type = QueryType::USER_INFO;
    msgResponse << response;
    sendResponse(connection, msgResponse);
}

void Server::findFriendsStatuses(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);
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
            User* user = m_db.getUser(line);
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
                // This must not happen
                logins.push_back(line);
                statuses.push_back("requested status of unknown user");
            }
        }
    }

    std::string response = m_sender.get_friendsStatusesSuccessStr(logins, statuses);
    net::message<QueryType> msgResponse;
    msgResponse.header.type = QueryType::FRIENDS_STATUSES;
    msgResponse << response;

    sendResponse(connection, msgResponse);
}

void Server::sendPendingMessages(connectionT connection) {
    auto it = std::find_if(m_map_online_users.begin(), m_map_online_users.end(), [connection](const auto& pair) {
        return pair.second->getConnection() == connection;
    });

    if (it != m_map_online_users.end()) {
        User* user = it->second;

        auto packets = m_db.getCollected(user->getLogin());
        std::unordered_map<std::string, std::vector<net::file<QueryType>>> filesMap;
        for (auto& [packet, type] : packets) {
            if (type == QueryType::FILE_PREVIEW) {
                std::istringstream iss(packet);

                std::string myLogin;
                std::getline(iss, myLogin);

                std::string friendLogin;
                std::getline(iss, friendLogin);

                std::string fileName;
                std::getline(iss, fileName);

                std::string fileId;
                std::getline(iss, fileId);

                std::string fileSize;
                std::getline(iss, fileSize);

                std::string fileTimestamp;
                std::getline(iss, fileTimestamp);

                std::string messageBegin;
                std::getline(iss, messageBegin);

                std::string caption;
                std::string line;
                while (std::getline(iss, line)) {
                    if (line == "MESSAGE_END") {
                        break;
                    }
                    else {
                        caption += line;
                        caption += '\n';
                    }
                }
                caption.pop_back();

                std::string filesCountInBlobStr;
                std::getline(iss, filesCountInBlobStr);
                size_t filesCountInBlob = std::stoi(filesCountInBlobStr);

                std::string blobUID;
                std::getline(iss, blobUID);

                const std::string filePath = "ReceivedFiles/" + fileId;
                size_t dotPos = fileName.find_last_of('.');

                std::string fullPath;
                if (dotPos != std::string::npos && dotPos + 1 < fileName.length()) {
                    std::string extension = fileName.substr(dotPos + 1);
                    fullPath = filePath + "." + extension;
                }

                std::ifstream fileStream(fullPath);
                bool isPresent = fileStream.good();

                if (isPresent) {
                    net::file<QueryType> file;
                    file.blobUID = blobUID;
                    file.caption = caption;
                    file.filePath = fullPath;
                    file.fileName = fileName;
                    file.filesInBlobCount = filesCountInBlob;
                    file.fileSize = std::stoi(fileSize);
                    file.id = fileId;
                    file.receiverLogin = friendLogin;
                    file.senderLogin = myLogin;
                    file.timestamp = fileTimestamp;
                        
                    if (file.fileSize > 100000000) {
                        std::string filePreviewStr = m_sender.get_filePreviewStr(file.senderLogin, file.receiverLogin, file.fileName, file.id, std::to_string(file.fileSize), file.timestamp, file.caption, file.blobUID, file.filesInBlobCount);
                        net::message<QueryType> msgResponse;
                        msgResponse.header.type = QueryType::FILE_PREVIEW;
                        msgResponse << filePreviewStr;
                        sendResponse(user->getConnection(), msgResponse);
                    }
                    else {
                        sendFile(user->getFilesConnection(), file);
                    }
                }
            }
            else {
                net::message<QueryType> msgResponse;
                msgResponse.header.type = type;
                msgResponse << packet;
                sendResponse(user->getConnection(), msgResponse);
            }
        }

        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::ALL_PENDING_MESSAGES_WERE_SENT;
        sendResponse(user->getConnection(), msgResponse);
    }
}

void Server::authorizeUser(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string login;
    std::getline(iss, login);

    std::string passwordHash;
    std::getline(iss, passwordHash);

    QueryType type = QueryType::_;

    if (m_map_online_users.find(login) != m_map_online_users.end()) {
        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::AUTHORIZATION_FAIL;
        type = QueryType::AUTHORIZATION_FAIL;
        sendResponse(connection, msgResponse);
        return;
    }

    bool isAuthorized = m_db.checkPassword(login, passwordHash);
    if (isAuthorized) {
        User* user = m_db.getUser(login);

        if (user == nullptr) {
            std::cout << "can't get user info";
        }

        else {
            user->setConnection(connection);
            user->setLastSeenToOnline();
            m_map_online_users[login] = user;

            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::AUTHORIZATION_SUCCESS;
            type = QueryType::AUTHORIZATION_SUCCESS;
            sendResponse(connection, msgResponse);
        }
    }
    else {
        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::AUTHORIZATION_FAIL;
        type = QueryType::AUTHORIZATION_FAIL;
        sendResponse(connection, msgResponse);
    }
}

void Server::registerUser(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string login;
    std::getline(iss, login);

    std::string name;
    std::getline(iss, name);

    std::string passwordHash;
    std::getline(iss, passwordHash);

    if (m_db.getUser(login) == nullptr) {;
        User* user = new User(login, passwordHash, name, false, Photo(), connection);
        user->setLastSeenToOnline();
        m_map_online_users[login] = user;

        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::REGISTRATION_SUCCESS;

        sendResponse(connection, msgResponse);
        m_db.addUser(login, name, user->getLastSeen(), passwordHash);
    }
    else {
        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::REGISTRATION_FAIL;
        sendResponse(connection, msgResponse);
    }
}

void Server::createChat(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string myLogin;
    std::getline(iss, myLogin);

    std::string friendLogin;
    std::getline(iss, friendLogin);

    QueryType responseType;
    std::string response;
    if (myLogin == friendLogin) {
        responseType = QueryType::CHAT_CREATE_FAIL;
    }
    else {
        setlocale(LC_ALL, "ru");

        User* user = m_db.getUser(friendLogin);
        if (user == nullptr) {
            responseType = QueryType::CHAT_CREATE_FAIL;
        }
        else {
            responseType = QueryType::CHAT_CREATE_SUCCESS;
            response = m_sender.get_chatCreateSuccessStr(user);
        }

        delete user;
    }

    net::message<QueryType> msgResponse;
    msgResponse.header.type = responseType;
    if (responseType == QueryType::CHAT_CREATE_SUCCESS) {
        msgResponse << response;
    }
    sendResponse(connection, msgResponse);
}

void Server::updateUserName(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string login;
    std::getline(iss, login);

    std::string newName;
    std::getline(iss, newName);

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

    auto it = m_map_online_users.find(login);
    User* user = it->second;
    user->setName(newName);
    
    m_db.updateUserName(login, newName);

    for (auto friendLogin : logins) {
        auto it = m_map_online_users.find(friendLogin);
        if (it != m_map_online_users.end()) {
            User* userTo = it->second;

            std::string packetU = m_sender.get_userInfoPacket(user);
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::USER_INFO;
            msgResponse << packetU;
            sendResponse(userTo->getConnection(), msgResponse);
        }
        else {
            std::string packetU = m_sender.get_userInfoPacket(user);
            m_db.collect(friendLogin, packetU, QueryType::USER_INFO);
        }
    }
}

void Server::bindFilesConnectionToUser(files_connectionT filesConnection, std::string login) {
    auto it = std::find_if(m_map_online_users.begin(), m_map_online_users.end(), [&login, this](const auto& pair) {
        return pair.first == login;
    });

    if (it != m_map_online_users.end()) {
        auto& [login, user] = *it;
        user->setFilesConnection(filesConnection);
        sendPendingMessages(user->getConnection());
    }
}

void Server::updateUserPassword(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string login;
    std::getline(iss, login);

    std::string newPasswordHash;
    std::getline(iss, newPasswordHash);

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

    auto it = m_map_online_users.find(login);
    if (it != m_map_online_users.end()) {
        User* user = it->second;
        user->setPassword(newPasswordHash);
    }
    m_db.updateUserPassword(login, newPasswordHash);
}

void Server::updateUserPhoto(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string login;
    std::getline(iss, login);

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

    std::string sizeStr;
    std::getline(iss, sizeStr);
    size_t size = std::stoi(sizeStr);

    std::string photoStr;
    std::getline(iss, photoStr);
    

    Photo photo = Photo::deserialize(base64_decode(photoStr), std::stoi(sizeStr), login);

    m_db.updateUserPhoto(login, photo, size);

    auto it = m_map_online_users.find(login);
    User* user = it->second;
    user->setPhoto(photo);

    for (auto friendLogin : logins) {
        auto it = m_map_online_users.find(friendLogin);
        if (it != m_map_online_users.end()) {
            User* userTo = it->second;

            std::string packetU = m_sender.get_userInfoPacket(user);
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::USER_INFO;
            msgResponse << packetU;
            sendResponse(userTo->getConnection(), msgResponse);
        }
        else {
            std::string packetU = m_sender.get_userInfoPacket(user);
            m_db.collect(friendLogin, packetU, QueryType::USER_INFO);
        }
    }
}

void Server::updateUserLogin(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string oldLogin;
    std::getline(iss, oldLogin);

    std::string newLogin;
    std::getline(iss, newLogin);

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

    auto mapIt = m_map_online_users.find(oldLogin);
    if (mapIt == m_map_online_users.end()) {
        return;
    }

    User* user = mapIt->second;

    m_db.updateUserLogin(oldLogin, newLogin);
 
    
    auto node = m_map_online_users.extract(mapIt);
    node.key() = newLogin;
    m_map_online_users.insert(std::move(node));
    

    for (auto friendLogin : logins) {
        if (auto it = m_map_online_users.find(friendLogin); it != m_map_online_users.end()) {
            std::string packetU = m_sender.get_userInfoPacket(user, newLogin);
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::USER_INFO;
            msgResponse << packetU;
            sendResponse(it->second->getConnection(), msgResponse);
        }
        else {
            std::string packetU = m_sender.get_userInfoPacket(user, newLogin);
            m_db.collect(friendLogin, packetU, QueryType::USER_INFO);
        }
    }

    user->setLogin(newLogin);
}


void Server::sendResponse(connectionT connection, net::message<QueryType>& msg) {
    msg.header.size = msg.size();
    sendMessage(connection, msg);
}



// errors
void Server::onSendMessageError(std::error_code ec, net::message<QueryType> unsentMessage) {
    std::string messageStr;
    unsentMessage >> messageStr;
    std::istringstream iss(messageStr);

    QueryType type = unsentMessage.header.type;

    if (type == QueryType::MESSAGE) {
        std::string friendLogin;
        std::getline(iss, friendLogin);

        m_db.collect(friendLogin, iss.str(), QueryType::MESSAGE);
    }
    else if (type == QueryType::MESSAGES_READ_CONFIRMATION) {
        std::string friendLogin;
        std::getline(iss, friendLogin);

        m_db.collect(friendLogin, iss.str(), QueryType::MESSAGES_READ_CONFIRMATION);
    }

    handleError(ec);
}

void Server::onSendFileError(std::error_code ec, net::file<QueryType> unsentFile) {
    std::string filePreviewStr = m_sender.get_filePreviewStr(unsentFile.senderLogin, unsentFile.receiverLogin, std::filesystem::path(unsentFile.filePath).filename().string(), unsentFile.id, std::to_string(unsentFile.fileSize), unsentFile.timestamp, unsentFile.caption, unsentFile.blobUID, unsentFile.filesInBlobCount);
    m_db.collect(unsentFile.receiverLogin, filePreviewStr, QueryType::FILE_PREVIEW);

    handleError(ec);
}


void Server::onReceiveMessageError(connectionT connection, std::error_code ec) {
    onClientDisconnect(connection);
    handleError(ec);
}

void Server::onReceiveFileError(std::error_code ec, net::file<QueryType> unreadFile) {
    handleError(ec);
}

void Server::handleError(std::error_code ec) {
    if (ec == asio::error::connection_reset) {
        // also happends on regular disconnects, so it's commented for better performance
        /*
        std::cerr << "Client forcibly closed connection during file transfer: "
             << std::endl;
        */
    }
    else if (ec == asio::error::eof) {
        std::cerr << "Client disconnected during file transfer: "
             << std::endl;
    }
    else if (ec == asio::error::operation_aborted) {
        std::cerr << "File transfer cancelled by server: "
             << std::endl;
    }
    else if (ec == asio::error::timed_out) {
        std::cerr << "Network timeout during file transfer: "
            << std::endl;
    }
    else if (ec == asio::error::network_down ||
        ec == asio::error::network_reset ||
        ec == asio::error::host_unreachable) {
        std::cerr << "Network problem detected during file transfer: "
            << ec.message() << std::endl;
    }
    else {
        std::cerr << "Unknown error during file transfer (" << ec << "): "
            << ec.message() << ", file: " <<  std::endl;
    }

    if (!hasInternetConnection()) {
        stopServer();
    }
}

void Server::onConnectError(std::error_code ec) {
    handleError(ec);
}

void Server::onFileSent(net::file<QueryType> sentFile) {
    std::filesystem::remove(sentFile.filePath);
}

bool Server::hasInternetConnection() {
#ifdef _WIN32
    const char* ping_cmd = "ping -n 1 1.1.1.1 > nul";
#else
    const char* ping_cmd = "ping -c 1 1.1.1.1 > /dev/null";
#endif

    int result = std::system(ping_cmd);
    return (result == 0);
}