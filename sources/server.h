#pragma once

#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <mutex>
#include <stack>


#include "database.h"
#include "blob.h"
#include "queryType.h"
#include "sender.h"
#include "user.h"
#include "net.h"

typedef std::shared_ptr<net::connection<QueryType>> connectionT;
typedef net::message<QueryType> MessageT;

class Server : public net::server_interface<QueryType> {
public:
    Server(int port);
    void startServer();
    void stopServer();

private:
    void processIncomingMessagesQueue();

    void onFileSent(net::file<QueryType> sentFile) override;

    // errors
    void onSendMessageError(std::error_code ec, net::message<QueryType> unsentMessage) override;
    void onSendFileError(std::error_code ec, net::file<QueryType> unsentFile) override;

    void onReadMessageError(connectionT connection, std::error_code ec) override;
    void onReadFileError(std::error_code ec, net::file<QueryType> unreadFile) override;

    void onConnectError(std::error_code ec) override;


    void onMessage(connectionT connection, MessageT msg) override;
    void onFile(net::file<QueryType> file) override;

    void onClientDisconnect(connectionT connection) override;
    bool onClientConnect(connectionT connection) override;



    void prepareToReceiveFile(connectionT connection, const std::string& stringPacket);
    void bindFilesConnectionToUser(connectionT connection, const std::string& stringPacket);
    void sendFileToUser(connectionT connection, net::file<QueryType>& file, bool isRequested);
    void onSendMeFile(connectionT connection, const std::string& stringPacket);

    void sendResponse(connectionT connection, net::message<QueryType>& msg);
    void sendPendingMessages(connectionT connection);

    void authorizeUser(connectionT connection, const std::string& stringPacket);
    void registerUser(connectionT connection, const std::string& stringPacket);

    void createChat(connectionT connection, const std::string& stringPacket);
    void verifyPassword(connectionT connection, const std::string& stringPacket);
    void checkNewLogin(connectionT connection, const std::string& stringPacket);
    void findUser(connectionT connection, const std::string& stringPacket);

    void updateUserName(connectionT connection, const std::string& stringPacket);
    void updateUserPassword(connectionT connection, const std::string& stringPacket);
    void updateUserPhoto(connectionT connection, const std::string& stringPacket);
    void updateUserLogin(connectionT connection, const std::string& stringPacket);

    void returnUserInfo(connectionT connection, const std::string& stringPacket);
    void findFriendsStatuses(connectionT connection, const std::string& stringPacket);

    void broadcastUserStatus(connectionT connection, const std::string& stringPacket);

    void handleBroadcast(connectionT connection, const std::string& stringPacket, QueryType type);
    void handleGet(connectionT connection, const std::string& stringPacket, QueryType type);
    void handleRpl(connectionT connection, const std::string& stringPacket, QueryType type);

    std::string rebuildRemainingStringFromIss(std::istringstream& iss);

    bool hasInternetConnection();
    void handleError(std::error_code ec);
    void handleFileBlobsOnInternetConnectionFail();


    void trySendNewBlob(const std::string& login);

private:
    std::thread                         m_worker_thread;

    SendStringsGenerator                m_sender;
    Database                            m_db;

    std::string                         m_ipAddress;
    int                                 m_port;

    std::unordered_map<std::string, User*> m_map_online_users;

    // login to pair of "is able to start sending process immediately flag" and map of blob UID to blob
    std::unordered_map<std::string, std::pair<bool, std::unordered_map<std::string, filesBlob<QueryType>>>> m_map_pending_files_blobs;
};