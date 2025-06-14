#pragma once

#include "net_common.h"
#include "net_safe_deque.h"
#include "net_message.h"
#include "net_connection.h"
#include "net_connection_type_resolver.h"

namespace net {
    template<typename T>
    class server_interface {
    public:
        server_interface(uint16_t port)
            : m_asio_acceptor(m_asio_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {
        }

        virtual ~server_interface() {
            stop();
        }

        bool start() {
            try {
                waitForClientConnections();
                m_context_thread = std::thread([this]() {m_asio_context.run(); });
            }
            catch (std::runtime_error e) {
                std::cout << "[SERVER] Start Error: " << e.what() << "\n";
            }

            std::cout << "[SERVER] Started!\n";
            return true;
        }

        void stop() {
            m_asio_context.stop();

            if (m_context_thread.joinable())
                m_context_thread.join();

            std::cout << "[SERVER] Stopped!\n";
        }

        void waitForClientConnections() {
            m_asio_acceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
                if (!ec) {
                    std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

                    if (m_resolver != nullptr) {
                        delete m_resolver;
                    }
                    m_resolver = new connection_type_resolver(
                        m_asio_context,
                        std::move(socket),
                        [this](std::error_code ec) { onConnectError(ec); },
                        [this](asio::ip::tcp::socket socket, connection_type type, std::optional<std::string> login) {
                            createConnection(std::move(socket), type, login);
                        }
                    );
                }
                else {
                    std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
                }

                waitForClientConnections();
                });
        }

        void sendMessage(std::shared_ptr<connection<T>> connection, const message<T>& msg) {
            if (isConnected(connection))
                connection->send(msg);
            else {
                onClientDisconnect(connection);
                connection->disconnect();
                removeConnection(connection);
            }
        }

        void sendFile(std::shared_ptr<files_connection<T>> filesConnection, net::file<T> file) {
            if (isConnected(filesConnection))
                filesConnection->sendFile(file);
            else {
                filesConnection->disconnect();
                removeConnection(filesConnection);
            }
        }

        void broadcastMessage(const message<T>& msg, std::shared_ptr<connection<T>> connectionToIgnore = nullptr) {
            bool isInvalidConnectionAppears = false;

            for (auto it = m_set_connections.begin(); it != m_set_connections.end(); ) {
                auto& connVariant = **it;
                bool isConnected = std::visit([](auto&& conn) { return conn->isConnected(); }, connVariant);

                if (isConnected) {
                    bool is_connection_to_ignore = std::visit(
                        [&connectionToIgnore](auto&& conn) {
                            using T = std::decay_t<decltype(conn)>;
                            if constexpr (std::is_same_v<T, std::shared_ptr<connection<T>>>) {
                                return conn == connectionToIgnore;
                            }
                            return false;
                        }, connVariant);

                    if (!is_connection_to_ignore) {
                        std::visit([&msg](auto&& conn) {
                            if constexpr (std::is_same_v<std::decay_t<decltype(conn)>, std::shared_ptr<connection<T>>>) {
                                conn->send(msg);
                            }
                            }, connVariant);
                        ++it;
                    }
                    else {
                        std::visit([this](auto&& conn) { onClientDisconnect(conn); }, connVariant);
                        it = m_set_connections.erase(it);
                        isInvalidConnectionAppears = true;
                    }
                }
                else {
                    std::visit([this](auto&& conn) { onClientDisconnect(conn); }, connVariant);
                    it = m_set_connections.erase(it);
                    isInvalidConnectionAppears = true;
                }
            }
        }

        void update(size_t maxMessagesCount = std::numeric_limits<unsigned long long>::max()) {
            size_t processedMessages = 0;

            while (true) {
                if (!m_safe_deque_of_incoming_files.empty()) {
                    net::owned_file<T> file = m_safe_deque_of_incoming_files.pop_front();
                    onFile(std::move(file.file));
                }

                if (!m_safe_deque_incoming_messages.empty() && processedMessages < maxMessagesCount) {
                    net::owned_message<T> msg = m_safe_deque_incoming_messages.pop_front();
                    onMessage(msg.remote, std::move(msg.msg));
                    processedMessages++;
                }

                std::this_thread::yield();
            }
        }

        static bool isConnected(std::shared_ptr<connection<T>> connection) {
            return connection && connection->isConnected();
        }

        static bool isConnected(std::shared_ptr<files_connection<T>> connection) {
            return connection && connection->isConnected();
        }

    private:
        template<typename ConnectionType>
        void removeConnection(std::shared_ptr<ConnectionType> connection) {
            auto it = std::find_if(m_set_connections.begin(), m_set_connections.end(),
                [&connection](const std::shared_ptr<connection_variant>& connVariant) {
                    if constexpr (std::is_same_v<ConnectionType, net::connection<T>>) {
                        if (auto* connPtr = std::get_if<std::shared_ptr<net::connection<T>>>(&*connVariant)) {
                            return *connPtr == connection;
                        }
                    }
                    else if constexpr (std::is_same_v<ConnectionType, files_connection<T>>) {
                        if (auto* connPtr = std::get_if<std::shared_ptr<files_connection<T>>>(&*connVariant)) {
                            return *connPtr == connection;
                        }
                    }
                    return false;
                });

            if (it != m_set_connections.end()) {
                m_set_connections.erase(it);
            }
        }

        void createConnection(asio::ip::tcp::socket socket, connection_type type, std::optional<std::string> login) {
            if (type == connection_type::files) {
                std::shared_ptr<files_connection<T>> newFilesConnection = std::make_shared<files_connection<T>>(
                    owner::server,
                    m_asio_context,
                    std::move(socket),
                    m_safe_deque_of_incoming_files,
                    [this](std::error_code ec, net::file<T> unreceivedFile) { onReceiveFileError(ec, unreceivedFile); },
                    [this](std::error_code ec, net::file<T> unsentFile) { onSendFileError(ec, unsentFile); },
                    [this](net::file<T> file) { onFileSent(file); },
                    std::nullopt,
                    std::nullopt
                );

                connection_variant variant = newFilesConnection;
                if (isConnectionAllowed(variant)) {
                    auto connection = std::make_shared<connection_variant>(std::move(newFilesConnection));
                    m_set_connections.emplace_back(connection);

                    if (auto* filesConn = std::get_if<std::shared_ptr<files_connection<QueryType>>>(&(*connection))) {
                        bindFilesConnectionToUser(*filesConn, std::move(login.value()));
                    }
                    else {
                        std::cout << "[-----] Error: Not a files_connection\n";
                    }
                }
                else {
                    std::cout << "[-----] Connection Denied\n";
                }
            }
            else {
                std::shared_ptr<connection<T>> newMessagesConnection = std::make_shared<connection<T>>(
                    owner::server,
                    m_asio_context,
                    std::move(socket),
                    m_safe_deque_incoming_messages,
                    [this](std::error_code ec, net::message<T> unsentMessage) { onSendMessageError(ec, unsentMessage); },
                    [this](std::shared_ptr<connection<T>> connection, std::error_code ec) { onReceiveMessageError(connection, ec); }
                );

                connection_variant variant = newMessagesConnection;
                if (isConnectionAllowed(variant)) {
                    m_set_connections.emplace_back(std::make_shared<connection_variant>(std::move(newMessagesConnection)));
                }
                else {
                    std::cout << "[-----] Connection Denied\n";
                }
            }
        }

    protected:
        using connection_variant = std::variant<
            std::shared_ptr<files_connection<T>>,
            std::shared_ptr<connection<T>>
        >;

        virtual void onMessage(std::shared_ptr<connection<T>> connection, message<T> msg) = 0;
        virtual void onFile(net::file<T> file) = 0;
        virtual void onFileSent(net::file<T> sentFile) = 0;
        virtual void bindFilesConnectionToUser(std::shared_ptr<files_connection<T>> filesConnection, std::string login) = 0;

        virtual bool isConnectionAllowed(connection_variant& connVariant) = 0;
        virtual void onClientDisconnect(std::shared_ptr<connection<T>> connection) = 0;

        //errors
        virtual void onSendMessageError(std::error_code ec, net::message<T> unsentMessage) = 0;
        virtual void onReceiveMessageError(std::shared_ptr<connection<T>> connection, std::error_code ec) = 0;

        virtual void onSendFileError(std::error_code ec, net::file<T> unsentFile) = 0;
        virtual void onReceiveFileError(std::error_code ec, net::file<T> unreadFile) = 0;

        virtual void onConnectError(std::error_code ec) = 0;

    protected:
        safe_deque<owned_message<T>> m_safe_deque_incoming_messages;
        safe_deque<owned_file<T>> m_safe_deque_of_incoming_files;
        std::deque<std::shared_ptr<connection_variant>> m_set_connections;
        connection_type_resolver* m_resolver = nullptr;

        asio::io_context m_asio_context;
        std::thread m_context_thread;
        asio::ip::tcp::acceptor m_asio_acceptor;
    };
}