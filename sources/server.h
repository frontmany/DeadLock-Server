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
#include "packetsBuilder.h"
#include "user.h"
#include "net.h"

#include "rsa.h"

typedef std::shared_ptr<net::connection<QueryType>> connectionT;
typedef std::shared_ptr<net::files_connection<QueryType>> files_connectionT;
typedef net::message<QueryType> MessageT;

class Server : public net::server_interface<QueryType> {
public:
    Server(int port);
    void startServer();
    void stopServer();
    void loadKeys();

    static net::file<QueryType> constructFileFromPacket(const std::string& packet);

private:
    void runUpdateChecker();
    void processIncomingMessagesQueue();

    void onFileSent(net::file<QueryType> sentFile) override;
    void onMessage(connectionT connection, MessageT msg) override;
    void onFile(net::file<QueryType> file) override;
    void onSendMeFile(connectionT connection, const std::string& stringPacket);
    void onUpdateRequested(connectionT connection, const std::string& stringPacket);

    void onClientDisconnect(connectionT connection) override;
    bool isConnectionAllowed(connection_variant& connVariant) override;

    // errors
    void onSendMessageError(std::error_code ec, net::message<QueryType> unsentMessage) override;
    void onReceiveMessageError(connectionT connection, std::error_code ec) override;
    void onSendFileError(std::error_code ec, net::file<QueryType> unsentFile) override;
    void onReceiveFileError(std::error_code ec, net::file<QueryType> unreadFile) override;
    void onConnectError(std::error_code ec) override;
    void bindFilesConnectionToUser(files_connectionT filesConnection, std::string login) override;

    void sendResponse(connectionT connection, net::message<QueryType>& msg);
    void sendPendingMessages(connectionT connection);
    void sendUpdateOfferPacket();

    void onAfterRegistrationInfo(connectionT connection, const std::string& stringPacket);
    void onPublicKey(connectionT connection, const std::string& stringPacket);
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
    void returnUserInfoAndUpdateKey(connectionT connection, const std::string& stringPacket);
    void findFriendsStatuses(connectionT connection, const std::string& stringPacket);

    void broadcastUserStatus(connectionT connection, const std::string& stringPacket);

    void handleBroadcast(connectionT connection, const std::string& stringPacket, QueryType type);
    void handleGet(connectionT connection, const std::string& stringPacket, QueryType type);
    void handleRpl(connectionT connection, const std::string& stringPacket, QueryType type);


    std::string getLatestVersionNumber();
    std::string generateEncryptionPart(const std::string& salt);
    std::string rebuildRemainingStringFromIss(std::istringstream& iss);
    void sendBlob(const Blob& blob, const std::string& loginHash);
    void static replaceLineInPacket(PacketData& packetData, size_t lineNumber, const std::string& newLineContent);


    bool hasInternetConnection();
    void handleError(std::error_code ec);

private:
    std::recursive_mutex m_map_mutex;
    std::thread m_update_checker_thread;
    PacketsBuilder m_packets_builder;
    Database m_db;

    const char* m_versionsListPath = "./versions/versionsList.txt";
    const char* m_folder_name = "versions";
    std::string m_ipAddress;
    int m_port;

    std::unordered_map<std::string, User*> m_map_online_users;
};