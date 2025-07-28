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

#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#include "database.h"
#include "blob.h"
#include "queryType.h"
#include "packetsBuilder.h"
#include "user.h"
#include "net.h"
#include "rsa.h"

class Server : public net::ServerInterface {
public:
    Server(int port);
    void startServer();
    void stopServer();
    void loadKeys();

private:
    void runUpdateChecker();
    void processIncomingMessagesQueue();

    void onMessage(net::Connection* connection, net::Message& msg) override;
    void onFile(net::FileMetadata file) override;
    void onFileSent(net::FileMetadata sentFile) override;
    void bindFilesConnectionToUser(net::FilesConnection* filesConnection, std::string login)override;
    bool isConnectionAllowed(net::Connection* connection) override;
    void onDisconnect(const std::string& ownerLoginHash) override;
    void onSendMessageError(std::error_code ec, net::Message& unsentMessage) override;
    void onSendFileError(std::error_code ec, net::FileMetadata unsentFile) override;
    void onReceiveFileError(std::error_code ec, std::optional<net::FileMetadata> unreadFile) override;


    void sendPendingMessages(const std::string& loginHash);
    void sendUpdateOfferPacket();

    net::FileMetadata constructFileFromPacket(const std::string& packet);
    void onUpdateRequested(net::Connection* connection, const std::string& stringPacket);
    void onSendMeFile(net::Connection* connection, const std::string& stringPacket);
    void onAfterRegistrationInfo(net::Connection* connection, const std::string& stringPacket);
    void onPublicKey(net::Connection* connection, const std::string& stringPacket);
    void authorizeUser(net::Connection* connection, const std::string& stringPacket);
    void registerUser(net::Connection* connection, const std::string& stringPacket);

    void createChat(net::Connection* connection, const std::string& stringPacket);
    void verifyPassword(net::Connection* connection, const std::string& stringPacket);
    void checkNewLogin(net::Connection* connection, const std::string& stringPacket);
    void findUser(net::Connection* connection, const std::string& stringPacket);

    void updateUserName(net::Connection* connection, const std::string& stringPacket);
    void updateUserPassword(net::Connection* connection, const std::string& stringPacket);
    void updateUserPhoto(net::Connection* connection, const std::string& stringPacket);
    void updateUserLogin(net::Connection* connection, const std::string& stringPacket);

    void returnUserInfo(net::Connection* connection, const std::string& stringPacket);
    void returnUserInfoAndUpdateKey(net::Connection* connection, const std::string& stringPacket);
    void findFriendsStatuses(net::Connection* connection, const std::string& stringPacket);

    void broadcastUserStatus(net::Connection* connection, const std::string& stringPacket);

    void handleBroadcast(net::Connection* connection, const std::string& stringPacket, QueryType type);
    void handleGet(net::Connection* connection, const std::string& stringPacket, QueryType type);
    void handleRpl(net::Connection* connection, const std::string& stringPacket, QueryType type);


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