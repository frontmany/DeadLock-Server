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

typedef std::shared_ptr<net::Connection> ConnectionPtr;
typedef std::shared_ptr<net::FilesConnection> FilesConnectionPtr;

class Server : public net::ServerInterface {
public:
    Server(int port);
    void startServer();
    void stopServer();
    void loadKeys();

private:
    void runUpdateChecker();
    void processIncomingMessagesQueue();

    void onMessage(ConnectionPtr connection, net::Message& msg) override;
    void onFile(net::FileMetadata file) override;
    void onFileSent(net::FileMetadata sentFile) override;
    void bindFilesConnectionToUser(FilesConnectionPtr filesConnection, std::string login)override;
    bool isConnectionAllowed(ConnectionPtr connection) override;
    void onDisconnect(const std::string& ownerLoginHash) override;
    void onSendMessageError(std::error_code ec, net::Message& unsentMessage) override;
    void onSendFileError(std::error_code ec, net::FileMetadata unsentFile) override;
    void onReceiveFileError(std::error_code ec, std::optional<net::FileMetadata> unreadFile) override;


    void sendPendingMessages(const std::string& loginHash);
    void sendUpdateOfferPacket();

    net::FileMetadata constructFileFromPacket(const std::string& packet);
    void onUpdateRequested(ConnectionPtr connection, const std::string& stringPacket);
    void onSendMeFile(ConnectionPtr connection, const std::string& stringPacket);
    void onAfterRegistrationInfo(ConnectionPtr connection, const std::string& stringPacket);
    void onPublicKey(ConnectionPtr connection, const std::string& stringPacket);
    void onReconnect(ConnectionPtr connection, const std::string& stringPacket);
    void authorizeUser(ConnectionPtr connection, const std::string& stringPacket);
    void registerUser(ConnectionPtr connection, const std::string& stringPacket);

    void createChat(ConnectionPtr connection, const std::string& stringPacket);
    void verifyPassword(ConnectionPtr connection, const std::string& stringPacket);
    void checkNewLogin(ConnectionPtr connection, const std::string& stringPacket);
    void findUser(ConnectionPtr connection, const std::string& stringPacket);

    void updateUserName(ConnectionPtr connection, const std::string& stringPacket);
    void updateUserPassword(ConnectionPtr connection, const std::string& stringPacket);
    void updateUserAvatar(const net::FileMetadata& fileAvatar);
    void updateUserLogin(ConnectionPtr connection, const std::string& stringPacket);

    void returnUserInfo(ConnectionPtr connection, const std::string& stringPacket);
    void returnUserInfoAndUpdateKey(ConnectionPtr connection, const std::string& stringPacket);
    void findFriendsStatuses(ConnectionPtr connection, const std::string& stringPacket);

    void broadcastUserStatus(ConnectionPtr connection, const std::string& stringPacket);

    void handleBroadcast(ConnectionPtr connection, const std::string& stringPacket, QueryType type);
    void handleGet(ConnectionPtr connection, const std::string& stringPacket, QueryType type);
    void handleRpl(ConnectionPtr connection, const std::string& stringPacket, QueryType type);


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
    int m_port;

    std::unordered_map<std::string, User*> m_map_online_users;
};