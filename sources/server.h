#pragma once

#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <mutex>
#include <stack>


#include "database.h"
#include "queryType.h"
#include "sender.h"
#include "user.h"
#include "net.h"

typedef std::shared_ptr<net::connection<QueryType>> connectionT;
typedef net::owned_message<QueryType> ownedMessageT;

class Server : public net::server_interface<QueryType> {
public:
    Server(int port);
    void startServer();
    void stopServer();

private:
    void processIncomingMessagesQueue();

    void onClientDisconnect(connectionT connection) override;
    bool onClientConnect(connectionT connection) override;
    void onMessage(connectionT connection, ownedMessageT& msg) override;

    void sendResponse(connectionT connection, net::message<QueryType>& msg);
    void sendPendingMessages(connectionT connection);

    void authorizeUser(connectionT connection, const std::string& stringPacket);
    void registerUser(connectionT connection, const std::string& stringPacket);

    void createChat(connectionT connection, const std::string& stringPacket);

    void updateUserName(connectionT connection, const std::string& stringPacket);
    void updateUserPassword(connectionT connection, const std::string& stringPacket);
    void updateUserPhoto(connectionT connection, const std::string& stringPacket);

    void returnUserInfo(connectionT connection, const std::string& stringPacket);
    void findFriendsStatuses(connectionT connection, const std::string& stringPacket);

    void broadcastUserStatus(connectionT connection, const std::string& stringPacket);

    void handleBroadcast(connectionT connection, const std::string& stringPacket, QueryType type);
    void handleGet(connectionT connection, const std::string& stringPacket, QueryType type);
    void handleRpl(connectionT connection, const std::string& stringPacket, QueryType type);

    std::string rebuildRemainingStringFromIss(std::istringstream& iss);

private:
    std::thread                         m_worker_thread;

    SendStringsGenerator                m_sender;
    Database                            m_db;

    std::string                         m_ipAddress;
    int                                 m_port;

    std::map<std::string, User*>  m_map_online_users;
};