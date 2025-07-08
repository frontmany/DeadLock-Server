#include "server.h"
#include "crypto.h"
#include "base64_my.h"

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
        m_db.updateUserLastSeen(user->getLoginHash(), crypto::RSAEncrypt(m_public_key, m_db.getCurrentDateTime()));
        m_map_online_users.erase(user->getLoginHash());
        delete user;
    }
    std::cout << "client disconnected\n";
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

    std::string encryptedKey;
    std::getline(iss, encryptedKey);
    CryptoPP::SecByteBlock key = crypto::RSADecryptKey(m_private_key, encryptedKey);

    std::string status;
    std::getline(iss, status);
    status = crypto::AESDecrypt(key, status);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string userLoginHash;
    while (std::getline(iss, userLoginHash)) {
        if (userLoginHash == "VEC_BEGIN") {
            continue;
        }
        if (userLoginHash == "VEC_END") {
            return;
        }
        else {
            auto it = m_map_online_users.find(userLoginHash);
            if (it == m_map_online_users.end()) {
                continue; 
            }
            else {
                net::message<QueryType> msg;
                msg.header.type = QueryType::STATUS;
                std::string messageStr = m_packets_builder.get_statusPacket(it->second->getPublicKey(), loginHash, status);
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
    else if (type == QueryType::LOAD_USER_INFO) {
        returnUserInfo(connection, stringPacket);
    }
    else if (type == QueryType::LOAD_MY_INFO) {
        returnUserInfoAndUpdateKey(connection, stringPacket);
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
    else if (type == QueryType::AFTER_RREGISTRATION_SEND_MY_INFO) {
        onAfterRegistrationInfo(connection, stringPacket);
    }
    else if (type == QueryType::PUBLIC_KEY) {
        onPublicKey(connection, stringPacket);
    }
}

void Server::handleRpl(connectionT connection, const std::string& stringPacket, QueryType type) {
    std::istringstream iss(stringPacket);

    std::string friendLoginHash;
    std::getline(iss, friendLoginHash);

    auto it = m_map_online_users.find(friendLoginHash);

    if (it == m_map_online_users.end()) {
        if (type == QueryType::MESSAGE) {
            m_db.collect(friendLoginHash, iss.str(), QueryType::MESSAGE);
        }
        else if (type == QueryType::MESSAGES_READ_CONFIRMATION) {
            m_db.collect(friendLoginHash, iss.str(), QueryType::MESSAGES_READ_CONFIRMATION);
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

    std::string encryptedKey;
    std::getline(iss, encryptedKey);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string fileId;
    std::getline(iss, fileId);

    std::string blobUID;
    std::getline(iss, blobUID);

    std::string friendLoginHash;
    std::getline(iss, friendLoginHash);

    std::string fileName;
    std::getline(iss, fileName);

    std::string fileSize;
    std::getline(iss, fileSize);

    std::string fileTimestamp;
    std::getline(iss, fileTimestamp);

    std::string caption;
    std::getline(iss, caption);

    std::string filesCountInBlob;
    std::getline(iss, filesCountInBlob);

    const std::string filePath = "ReceivedFiles/" + fileId + ".deadlock";
    std::ifstream fileStream(filePath);
    bool isPresent = fileStream.good();

    if (isPresent) {
        auto it = m_map_online_users.find(loginHash);
        auto& [userLoginHash, user] = *it;
        
        if (isPresent) {
            net::file<QueryType> file;
            file.blobUID = blobUID;
            file.caption = caption;
            file.filePath = filePath;
            file.fileName = fileName;
            file.filesInBlobCount = filesCountInBlob;
            file.fileSize = fileSize;
            file.id = fileId;
            file.receiverLoginHash = loginHash;
            file.senderLoginHash = friendLoginHash;
            file.timestamp = fileTimestamp;
            file.encryptedKey = encryptedKey;

            sendFile(user->getFilesConnection(), file);
        }
    }
    else {
        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::UNEXISTING_FILE;
        msgResponse << stringPacket;
        sendResponse(connection, msgResponse);
    }
}

void Server::onFile(net::file<QueryType> file) {
    auto it = m_map_online_users.find(file.receiverLoginHash);

    if (it == m_map_online_users.end()) {
        std::string filePreviewStr = m_packets_builder.get_filePreviewPacket(file.encryptedKey, file.senderLoginHash, file.receiverLoginHash, file.fileName, file.id, file.fileSize, file.timestamp, file.caption, file.blobUID, file.filesInBlobCount);
        m_db.collect(file.receiverLoginHash, filePreviewStr, QueryType::FILE_PREVIEW);
    }
    else {
        if (std::stoi(file.fileSize) > 100000000) { // 100mb
            std::string filePreviewStr = m_packets_builder.get_filePreviewPacket(file.encryptedKey, file.senderLoginHash, file.receiverLoginHash, file.fileName, file.id, file.fileSize, file.timestamp, file.caption, file.blobUID, file.filesInBlobCount);
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

void Server::findUser(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string encryptedKey;
    std::getline(iss, encryptedKey);
    CryptoPP::SecByteBlock key = crypto::RSADecryptKey(m_private_key, encryptedKey);

    std::string myLoginHash;
    std::getline(iss, myLoginHash);

    std::string searchText;
    std::getline(iss, searchText);
    searchText = crypto::AESDecrypt(key, searchText);

    std::vector<User*> vec;
    m_db.findUsers(m_private_key, myLoginHash, searchText, vec);

    net::message<QueryType> msgResponse;
    msgResponse.header.type = QueryType::FIND_USER_RESULTS;

    auto it = m_map_online_users.find(myLoginHash);
    User* user = it->second;

    std::string s = m_packets_builder.get_usersPacket(m_private_key, user->getPublicKey(),  vec);
    msgResponse << s;

    sendResponse(connection, msgResponse);
}

void Server::checkNewLogin(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string encryptedKey;
    std::getline(iss, encryptedKey);
    CryptoPP::SecByteBlock key = crypto::RSADecryptKey(m_private_key, encryptedKey);

    std::string oldLoginHash;
    std::getline(iss, oldLoginHash);

    std::string newLogin;
    std::getline(iss, newLogin);
    newLogin = crypto::AESDecrypt(key, newLogin);

    std::string newLoginHash = crypto::calculateHash(newLogin);
    bool isAvailable = m_db.checkNewLogin(newLoginHash);

    net::message<QueryType> msgResponse;
    if (isAvailable) {
        msgResponse.header.type = QueryType::NEW_LOGIN_SUCCESS;

        auto it = m_map_online_users.find(oldLoginHash);
        User* user = it->second;
        msgResponse << m_packets_builder.get_newLoginSuccessPacket(newLogin, user->getPublicKey());
    }
    else
        msgResponse.header.type = QueryType::NEW_LOGIN_FAIL;

    sendResponse(connection, msgResponse);
}

void Server::onAfterRegistrationInfo(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string encryptedKey;
    std::getline(iss, encryptedKey);
    CryptoPP::SecByteBlock key = crypto::RSADecryptKey(m_private_key, encryptedKey);

    std::string login;
    std::getline(iss, login);
    login = crypto::AESDecrypt(key, login);

    std::string name;
    std::getline(iss, name);
    name = crypto::AESDecrypt(key, name);


    auto it = m_map_online_users.find(crypto::calculateHash(login));
    auto& [loginHash, user] = *it;
    user->setLogin(login);
    user->setName(name);

    m_db.updateUserLoginOnly(loginHash, crypto::RSAEncrypt(m_public_key, login));
    m_db.updateUserName(loginHash, crypto::RSAEncrypt(m_public_key, name));
}

void Server::onPublicKey(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string publicKeyStr;
    std::getline(iss, publicKeyStr);
    CryptoPP::RSA::PublicKey publicKey = crypto::deserializePublicKey(publicKeyStr);

    auto it = m_map_online_users.find(loginHash);
    auto& [loginH, user] = *it;
    user->setPublicKey(publicKey);

    m_db.updateUserPublicKey(loginHash, crypto::serializePublicKey(publicKey));
}

void Server::verifyPassword(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string passwordHash;
    std::getline(iss, passwordHash);

    bool isPasswordsMatch = m_db.checkPassword(loginHash, passwordHash);

    net::message<QueryType> msgResponse;
    if (isPasswordsMatch) 
        msgResponse.header.type = QueryType::VERIFY_PASSWORD_SUCCESS;
    else
        msgResponse.header.type = QueryType::VERIFY_PASSWORD_FAIL;

    sendResponse(connection, msgResponse);
}

void Server::returnUserInfo(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string loginHashToSearch;
    std::getline(iss, loginHashToSearch);

    auto itUser = m_map_online_users.find(loginHash);
    User* user = m_db.getUser(m_private_key, loginHash);

    
    auto itSearch = m_map_online_users.find(loginHashToSearch);

    if (itSearch == m_map_online_users.end()) {
        User* userSearch = m_db.getUser(m_private_key, loginHashToSearch);
        if (userSearch != nullptr) {
            std::string response = m_packets_builder.get_userInfoPacket(m_private_key, userSearch, user->getPublicKey());
            
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::USER_INFO_SUCCESS;
            msgResponse << response;
            sendResponse(connection, msgResponse);
        }
        else {
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::USER_INFO_FAIL;
            sendResponse(connection, msgResponse);
        }
        delete userSearch;
    }
    else {
        User* userSearch = itSearch->second;
        std::string response = m_packets_builder.get_userInfoPacket(m_private_key, userSearch, user->getPublicKey());
        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::USER_INFO_SUCCESS;
        msgResponse << response;
        sendResponse(connection, msgResponse);
    }
}

void Server::returnUserInfoAndUpdateKey(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string newPublicKeyStr;
    std::getline(iss, newPublicKeyStr);
    CryptoPP::RSA::PublicKey newPublicKey = crypto::deserializePublicKey(newPublicKeyStr);

    auto it = m_map_online_users.find(loginHash);
    auto& [loginH, user] = *it;
    user->setPublicKey(newPublicKey);

    m_db.updateUserPublicKey(loginHash, crypto::serializePublicKey(newPublicKey));

    std::string response = m_packets_builder.get_MyInfoPacket(m_private_key, user);
    net::message<QueryType> msgResponse;
    msgResponse.header.type = QueryType::MY_INFO;
    msgResponse << response;
    sendResponse(connection, msgResponse);
}

void Server::findFriendsStatuses(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string vecBegin;
    std::getline(iss, vecBegin);

    std::vector<std::string> loginHashes;
    std::vector<std::string> statuses;

    std::string friendLoginHash;
    while (std::getline(iss, friendLoginHash)) {
        if (friendLoginHash == "VEC_END") {
            break;
        }
        else {
            User* user = m_db.getUser(m_private_key, friendLoginHash);
            if (user != nullptr) {
                loginHashes.push_back(user->getLoginHash());

                auto it = m_map_online_users.find(friendLoginHash);
                if (it == m_map_online_users.end()) {
                    statuses.push_back(user->getLastSeen());
                }
                else {
                    statuses.push_back("online");
                }
            }
            else {
                // This must not happen
                loginHashes.push_back(loginHash);
                statuses.push_back("requested status of unknown user");
            }
        }
    }

    auto it = m_map_online_users.find(loginHash);
    User* user = it->second;

    std::string response = m_packets_builder.get_friendsStatusesSuccessPacket(user->getPublicKey(), loginHashes, statuses);
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

        auto packets = m_db.getCollected(user->getLoginHash());
        std::unordered_map<std::string, std::vector<net::file<QueryType>>> filesMap;
        for (auto& [packet, type] : packets) {
            if (type == QueryType::FILE_PREVIEW) {
                std::istringstream iss(packet);

                std::string encryptedKey;
                std::getline(iss, encryptedKey);

                std::string fileId;
                std::getline(iss, fileId);

                std::string blobUID;
                std::getline(iss, blobUID);

                std::string receiverLoginHash;
                std::getline(iss, receiverLoginHash);

                std::string senderLoginHash;
                std::getline(iss, senderLoginHash);

                std::string fileName;
                std::getline(iss, fileName);

                std::string fileSize;
                std::getline(iss, fileSize);

                std::string fileTimestamp;
                std::getline(iss, fileTimestamp);

                std::string caption;
                std::getline(iss, caption);

                std::string filesCountInBlob;
                std::getline(iss, filesCountInBlob);

                const std::string filePath = "ReceivedFiles/" + fileId + ".deadlock";

                std::ifstream fileStream(filePath);
                bool isPresent = fileStream.good();

                if (isPresent) {
                    net::file<QueryType> file;
                    file.encryptedKey = encryptedKey;
                    file.blobUID = blobUID;
                    file.caption = caption;
                    file.filePath = filePath;
                    file.fileName = fileName;
                    file.filesInBlobCount = filesCountInBlob;
                    file.fileSize = fileSize;
                    file.id = fileId;
                    file.receiverLoginHash = receiverLoginHash;
                    file.senderLoginHash = senderLoginHash;
                    file.timestamp = fileTimestamp;
                        
                    if (std::stoi(file.fileSize) > 100000000) {
                        std::string filePreviewStr = m_packets_builder.get_filePreviewPacket(file.encryptedKey, file.senderLoginHash, file.receiverLoginHash, file.fileName, file.id, file.fileSize, file.timestamp, file.caption, file.blobUID, file.filesInBlobCount);
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

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string passwordHash;
    std::getline(iss, passwordHash);

    QueryType type = QueryType::_;

    if (m_map_online_users.find(loginHash) != m_map_online_users.end()) {
        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::AUTHORIZATION_FAIL;
        type = QueryType::AUTHORIZATION_FAIL;
        sendResponse(connection, msgResponse);
        return;
    }

    bool isAuthorized = m_db.checkPassword(loginHash, passwordHash);
    if (isAuthorized) {
        User* user = m_db.getUser(m_private_key, loginHash);

        if (user == nullptr) {
            std::cout << "can't get user info";
        }

        else {
            user->setConnection(connection);
            user->setLastSeenToOnline();
            m_map_online_users[loginHash] = user;

            net::message<QueryType> msgResponse;
            msgResponse << m_packets_builder.get_authorizationSuccessPacket(user->getEncryptionPart(), m_public_key);
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

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string passwordHash;
    std::getline(iss, passwordHash);

    if (m_db.getUser(m_private_key, loginHash) == nullptr) {
        std::string encryptionPart = generateEncryptionPart(loginHash);

        User* user = new User(loginHash, passwordHash, false, Photo(), connection);
        user->setLastSeenToOnline();
        user->setEncryptionPart(encryptionPart);
        m_map_online_users[loginHash] = user;

        bool  isAdded = m_db.addUser(loginHash, passwordHash, crypto::RSAEncrypt(m_public_key, encryptionPart), crypto::RSAEncrypt(m_public_key, user->getLastSeen()));
        if (!isAdded){
            std::cout << "error user was not added in db\n";
        }

        net::message<QueryType> msgResponse;
        msgResponse << m_packets_builder.get_authorizationSuccessPacket(user->getEncryptionPart(), m_public_key);
        msgResponse.header.type = QueryType::REGISTRATION_SUCCESS;
        sendResponse(connection, msgResponse);

    }
    else {
        net::message<QueryType> msgResponse;
        msgResponse.header.type = QueryType::REGISTRATION_FAIL;
        sendResponse(connection, msgResponse);
    }
}

void Server::createChat(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string loginHashToCreateChat;
    std::getline(iss, loginHashToCreateChat);

    auto it = m_map_online_users.find(loginHash);
    User* user = it->second;

    QueryType responseType;
    std::string response;
    if (loginHash == loginHashToCreateChat) {
        responseType = QueryType::CHAT_CREATE_FAIL;
    }
    else {
        setlocale(LC_ALL, "ru");

        User* userToCreateChat = m_db.getUser(m_private_key, loginHashToCreateChat);
        if (userToCreateChat == nullptr) {
            responseType = QueryType::CHAT_CREATE_FAIL;
        }
        else {
            responseType = QueryType::CHAT_CREATE_SUCCESS;
            response = m_packets_builder.get_chatCreateSuccessPacket(m_private_key, userToCreateChat, user->getPublicKey());
        }

        delete userToCreateChat;
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

    std::string encryptedKey;
    std::getline(iss, encryptedKey);
    CryptoPP::SecByteBlock key = crypto::RSADecryptKey(m_private_key, encryptedKey);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string newName;
    std::getline(iss, newName);
    newName = crypto::AESDecrypt(key, newName);

    std::string vecBegin;
    std::getline(iss, vecBegin);

    std::vector<std::string> loginHashes;
    std::string friendLoginHash;
    while (std::getline(iss, friendLoginHash)) {
        if (friendLoginHash == "VEC_END") {
            break;
        }
        else {
            loginHashes.push_back(friendLoginHash);
        }
    }

    auto it = m_map_online_users.find(loginHash);
    User* user = it->second;
    user->setName(newName);
    
    m_db.updateUserName(loginHash, crypto::RSAEncrypt(m_public_key, newName));

    for (auto& friendLoginHash : loginHashes) {
        auto it = m_map_online_users.find(friendLoginHash);
        if (it != m_map_online_users.end()) {
            User* userTo = it->second;

            std::string packetU = m_packets_builder.get_userInfoPacket(m_private_key, user, userTo->getPublicKey());
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::USER_INFO_SUCCESS;
            msgResponse << packetU;
            sendResponse(userTo->getConnection(), msgResponse);
        }
        else {
            User* userTo = m_db.getUser(m_private_key, friendLoginHash);
            std::string packetU = m_packets_builder.get_userInfoPacket(m_private_key, user, userTo->getPublicKey());
            m_db.collect(friendLoginHash, packetU, QueryType::USER_INFO_SUCCESS);
        }
    }
}

void Server::bindFilesConnectionToUser(files_connectionT filesConnection, std::string loginHash) {
    auto it = std::find_if(m_map_online_users.begin(), m_map_online_users.end(), [&loginHash, this](const auto& pair) {
        return pair.first == loginHash;
    });

    if (it != m_map_online_users.end()) {
        auto& [login, user] = *it;
        user->setFilesConnection(filesConnection);
        sendPendingMessages(user->getConnection());
    }
}

void Server::updateUserPassword(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string newPasswordHash;
    std::getline(iss, newPasswordHash);

    auto it = m_map_online_users.find(loginHash);
    if (it != m_map_online_users.end()) {
        User* user = it->second;
        user->setPasswordHash(newPasswordHash);
    }

    m_db.updateUserPassword(loginHash, newPasswordHash);
}

void Server::updateUserPhoto(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string encryptedKey;
    std::getline(iss, encryptedKey);
    CryptoPP::SecByteBlock key = crypto::RSADecryptKey(m_private_key, encryptedKey);

    std::string loginHash;
    std::getline(iss, loginHash);

    std::string vecBegin;
    std::getline(iss, vecBegin);

    std::vector<std::string> loginHashes;
    std::string line;
    while (std::getline(iss, line)) {
        if (line == "VEC_END") {
            break;
        }
        else {
            loginHashes.push_back(line);
        }
    }

    std::string isHasPhotoStr;
    std::getline(iss, isHasPhotoStr);
    isHasPhotoStr = crypto::AESDecrypt(key, isHasPhotoStr);

    std::string sizeStr;
    std::getline(iss, sizeStr);
    sizeStr = crypto::AESDecrypt(key, sizeStr);
    size_t size = std::stoi(sizeStr);

    std::string dataFirstPartStr;
    std::getline(iss, dataFirstPartStr);
    std::string dataSecondPartStr;
    std::getline(iss, dataSecondPartStr);
    
    auto photoOpt = Photo::deserialize(m_private_key, dataFirstPartStr + "\n" + dataSecondPartStr, loginHash);
    if (photoOpt) {
        Photo& photo = *photoOpt;  
        m_db.updateUserPhoto(m_public_key, loginHash, photo, size);
        auto it = m_map_online_users.find(loginHash);
        User* user = it->second;
        user->setPhoto(photo);

        for (auto& friendLoginHash : loginHashes) {
            auto it = m_map_online_users.find(friendLoginHash);
            if (it != m_map_online_users.end()) {
                User* userTo = it->second;

                std::string packetU = m_packets_builder.get_userInfoPacket(m_private_key, user, userTo->getPublicKey());
                net::message<QueryType> msgResponse;
                msgResponse.header.type = QueryType::USER_INFO_SUCCESS;
                msgResponse << packetU;
                sendResponse(userTo->getConnection(), msgResponse);
            }
            else {
                User* userTo = m_db.getUser(m_private_key, friendLoginHash);
                std::string packetU = m_packets_builder.get_userInfoPacket(m_private_key, user, userTo->getPublicKey());
                m_db.collect(friendLoginHash, packetU, QueryType::USER_INFO_SUCCESS);
            }
        }
    }
    else {
        std::cout << "(updateUserPhoto) error: photo deserialization error\n";
    }
}

void Server::updateUserLogin(connectionT connection, const std::string& stringPacket) {
    std::istringstream iss(stringPacket);

    std::string encryptedKey;
    std::getline(iss, encryptedKey);
    CryptoPP::SecByteBlock key = crypto::RSADecryptKey(m_private_key, encryptedKey);

    std::string oldLoginHash;
    std::getline(iss, oldLoginHash);

    std::string newLoginHash;
    std::getline(iss, newLoginHash);

    std::string newLogin;
    std::getline(iss, newLogin);
    newLogin = crypto::AESDecrypt(key, newLogin);

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

    auto mapIt = m_map_online_users.find(oldLoginHash);
    if (mapIt == m_map_online_users.end()) {
        return;
    }

    User* user = mapIt->second;

    m_db.updateUserLogin(m_public_key, oldLoginHash, newLogin);
    

    for (auto& friendLoginHash : logins) {
        if (auto it = m_map_online_users.find(friendLoginHash); it != m_map_online_users.end()) {
            std::string packetU = m_packets_builder.get_userInfoPacket(m_private_key, user, user->getPublicKey(), newLogin);
            net::message<QueryType> msgResponse;
            msgResponse.header.type = QueryType::USER_INFO_SUCCESS;
            msgResponse << packetU;
            sendResponse(it->second->getConnection(), msgResponse);
        }
        else {
            User* userTo = m_db.getUser(m_private_key, friendLoginHash);
            std::string packetU = m_packets_builder.get_userInfoPacket(m_private_key, user, userTo->getPublicKey(), newLogin);
            m_db.collect(friendLoginHash, packetU, QueryType::USER_INFO_SUCCESS);
        }
    }

    auto node = m_map_online_users.extract(mapIt);
    node.key() = newLoginHash;
    node.mapped()->setLogin(newLogin);
    node.mapped()->setLoginHash(newLoginHash);
    m_map_online_users.insert(std::move(node));
}



// essentials
std::string Server::rebuildRemainingStringFromIss(std::istringstream& iss) {
    std::string remainingStr;
    std::string line;
    while (std::getline(iss, line)) {
        remainingStr += line + '\n';
    }
    remainingStr.pop_back();
    return remainingStr;
}

void Server::sendResponse(connectionT connection, net::message<QueryType>& msg) {
    msg.header.size = msg.size();
    sendMessage(connection, msg);
}

std::string Server::generateEncryptionPart(const std::string& salt) {
    const std::string chars =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    const size_t length = 64;

    std::vector<unsigned> seed_data;
    seed_data.reserve(salt.size() + 1);

    for (char c : salt) {
        seed_data.push_back(static_cast<unsigned>(c));
    }

    seed_data.push_back(static_cast<unsigned>(std::time(nullptr)));

    std::seed_seq seed(seed_data.begin(), seed_data.end());
    std::mt19937 generator(seed);
    std::uniform_int_distribution<size_t> distribution(0, chars.size() - 1);

    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        result += chars[distribution(generator)];
    }

    return result;
}



// errors
void Server::onSendMessageError(std::error_code ec, net::message<QueryType> unsentMessage) {
    std::string messageStr;
    unsentMessage >> messageStr;
    std::istringstream iss(messageStr);

    QueryType type = unsentMessage.header.type;

    if (type == QueryType::MESSAGE) {
        std::string friendLoginHash;
        std::getline(iss, friendLoginHash);

        m_db.collect(friendLoginHash, iss.str(), QueryType::MESSAGE);
    }
    else if (type == QueryType::MESSAGES_READ_CONFIRMATION) {
        std::string friendLoginHash;
        std::getline(iss, friendLoginHash);

        m_db.collect(friendLoginHash, iss.str(), QueryType::MESSAGES_READ_CONFIRMATION);
    }

    handleError(ec);
}

void Server::onSendFileError(std::error_code ec, net::file<QueryType> unsentFile) {
    std::string filePreviewStr = m_packets_builder.get_filePreviewPacket(unsentFile.encryptedKey, unsentFile.senderLoginHash, unsentFile.receiverLoginHash, unsentFile.fileName, unsentFile.id, unsentFile.fileSize, unsentFile.timestamp, unsentFile.caption, unsentFile.blobUID, unsentFile.filesInBlobCount);
    m_db.collect(unsentFile.receiverLoginHash, filePreviewStr, QueryType::FILE_PREVIEW);

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

void Server::loadKeys() {
    try {
        std::ifstream pubFile("public_key.txt");
        if (!pubFile.is_open()) {
            throw std::runtime_error("Failed to open public key file");
        }
        std::string publicKeyStr((std::istreambuf_iterator<char>(pubFile)),
            std::istreambuf_iterator<char>());
        pubFile.close();

        std::ifstream privFile("private_key.txt");
        if (!privFile.is_open()) {
            throw std::runtime_error("Failed to open private key file");
        }
        std::string privateKeyStr((std::istreambuf_iterator<char>(privFile)),
            std::istreambuf_iterator<char>());
        privFile.close();

        m_public_key = crypto::deserializePublicKey(publicKeyStr);
        m_private_key = crypto::deserializePrivateKey(privateKeyStr);

    }
    catch (const std::exception& e) {
        std::cerr << "Error loading keys: " << e.what() << std::endl;
        throw;
    }
}