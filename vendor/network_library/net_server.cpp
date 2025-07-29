#include "net_server.h"

#include "net_connection.h"
#include "net_filesConnection.h"
#include "net_generatorID.h"

namespace net {
    ServerInterface::ServerInterface(uint16_t port)
        : m_asioAcceptor(m_asioContext,
            asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) 
    {
    }

    ServerInterface::~ServerInterface() {
        stop();
    }

    void ServerInterface::removeConnection(ConnectionPtr connection) {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_setConnections.erase(connection);
        
    }

    bool ServerInterface::start() {
        try {
            m_contextThread = std::thread([this]() {m_asioContext.run(); });
            waitForClientConnections();
        }
        catch (std::runtime_error e) {
            std::cout << "[SERVER] Start Error: " << e.what() << "\n";
        }

        std::cout << "[SERVER] Started!\n";
        return true;
    }

    void ServerInterface::stop() {
        m_asioContext.stop();

        if (m_contextThread.joinable())
            m_contextThread.join();

        std::cout << "[SERVER] Stopped!\n";
    }

    void ServerInterface::waitForClientConnections() {
        m_asioAcceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

                uint64_t newId = GeneratorID::getID();

                auto resolver = std::make_unique<net::ConnectionTypeResolver>(
                    m_asioContext,
                    std::move(socket),
                    newId,
                    [this](std::error_code ec, uint64_t resolverID) { onConnectError(ec, resolverID); },
                    [this](uint64_t resolverID, asio::ip::tcp::socket socket) {
                        createConnection(resolverID, std::move(socket));
                    },
                    [this](uint64_t resolverID, asio::ip::tcp::socket socket, std::string login) {
                        createFilesConnection(resolverID,std::move(socket), login);
                    }
                );

                m_mapResolvers.emplace(newId, std::move(resolver));
            }
            else {
                std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
            }

            waitForClientConnections();
        });
    }

    void ServerInterface::sendMessage(ConnectionPtr connection, const Message& msg) {
        connection->send(msg);
    }

    void ServerInterface::sendFile(FilesConnectionPtr filesConnection, FileMetadata file) {
        filesConnection->sendFile(file);
    }

    void ServerInterface::update(size_t maxMessagesCount) {
        size_t processedMessages = 0;

        while (true) {
            if (!m_safeDequeIncomingFiles.empty()) {
                FileMetadata file = m_safeDequeIncomingFiles.pop_front();
                onFile(std::move(file));
            }

            if (!m_safeDequeIncomingMessages.empty() && processedMessages < maxMessagesCount) {
                OwnedMessage ownedMessage = m_safeDequeIncomingMessages.pop_front();
                onMessage(ownedMessage.connection, ownedMessage.message);
                processedMessages++;
            }

            std::this_thread::yield();
        }
    }

    void ServerInterface::createConnection(uint64_t id, asio::ip::tcp::socket socket)
    {
        ConnectionPtr newMessagesConnection = std::make_shared<Connection>(
            m_asioContext,
            std::move(socket),
            m_safeDequeIncomingMessages,
            [this](std::error_code ec, Message unsentMessage) { onSendMessageError(ec, unsentMessage); },
            [this](std::string ownerLoginHash) { onDisconnect(ownerLoginHash); }
        );

        if (m_mapResolvers.contains(id)) {
            m_mapResolvers.erase(id);
        }

        if (isConnectionAllowed(newMessagesConnection)) {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_setConnections.insert(newMessagesConnection);
        }
        else {
            std::cout << "[-----] Connection Denied\n";
        }
    }

    void ServerInterface::createFilesConnection(uint64_t id, asio::ip::tcp::socket socket, const std::string& login)
    {
        FilesConnectionPtr newFilesConnection = std::make_shared<FilesConnection>(
            m_asioContext,
            std::move(socket),
            m_safeDequeIncomingFiles,
            [this](std::error_code ec, std::optional<FileMetadata> unreceivedFile) { onReceiveFileError(ec, unreceivedFile); },
            [this](std::error_code ec, FileMetadata unsentFile) { onSendFileError(ec, unsentFile); },
            [this](FileMetadata file) { onFileSent(file); },
            [this](std::string ownerLoginHash) { onDisconnect(ownerLoginHash); }
        );

        if (m_mapResolvers.contains(id)) {
            m_mapResolvers.erase(id);
        }

        bindFilesConnectionToUser(newFilesConnection, login);
    }

    void ServerInterface::onConnectError(std::error_code ec, uint64_t id) {
        if (m_mapResolvers.contains(id)) {
            m_mapResolvers.erase(id);
        }
        else {
            std::cout << "[-----] Resolver not found in map by id: " << id << "\n";
        }
    }
}