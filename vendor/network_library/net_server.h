#pragma once
#include "net_common.h"
#include "net_safeDeque.h"
#include "net_message.h"
#include "net_fileMetadata.h"
#include "net_connectionTypeResolver.h"



namespace net {
    class FilesConnection;
    class Connection;

    class ServerInterface {
    public:
        ServerInterface(uint16_t port);
        virtual ~ServerInterface();

        void waitForClientConnections();
        void sendMessage(Connection* connection, const Message& msg);
        void sendFile(FilesConnection* filesConnection, FileMetadata file);
        void update(size_t maxMessagesCount = std::numeric_limits<unsigned long long>::max());
        void removeConnection(Connection* connection);

        bool start();
        void stop();

    protected:
        virtual void onMessage(Connection* connection, Message& msg) = 0;
        virtual void onFile(FileMetadata file) = 0;
        virtual void onFileSent(FileMetadata sentFile) = 0;
        virtual void bindFilesConnectionToUser(FilesConnection* filesConnection, std::string login) = 0;
        virtual bool isConnectionAllowed(Connection* connection) = 0;
        virtual void onDisconnect(const std::string& ownerLoginHash) = 0;
        virtual void onSendMessageError(std::error_code ec, net::Message& msg) = 0;
        virtual void onSendFileError(std::error_code ec, FileMetadata unsentFile) = 0;
        virtual void onReceiveFileError(std::error_code ec, std::optional<FileMetadata> unreadFile) = 0;

        CryptoPP::RSA::PrivateKey m_privateKey;
        CryptoPP::RSA::PublicKey m_publicKey;


    private:
        void createConnection(asio::ip::tcp::socket socket);
        void createFilesConnection(asio::ip::tcp::socket socket, const std::string& login);
        void onConnectError(std::error_code ec, uint64_t id);


    private:
        SafeDeque<OwnedMessage> m_safeDequeIncomingMessages;
        SafeDeque<FileMetadata> m_safeDequeIncomingFiles;

        std::unordered_set<Connection*> m_setConnections;

        std::unordered_map<uint64_t, std::unique_ptr<ConnectionTypeResolver>> m_mapResolvers;

        asio::io_context m_asioContext;
        std::thread m_contextThread;
        asio::ip::tcp::acceptor m_asioAcceptor;
    };
}