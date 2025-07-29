#include "net_connectionTypeResolver.h"

namespace net {
    ConnectionTypeResolver::ConnectionTypeResolver(
        asio::io_context& asioContext,
        asio::ip::tcp::socket socket,
        uint64_t id,
        std::function<void(std::error_code, uint64_t)> errorCallback,
        std::function<void(uint64_t, asio::ip::tcp::socket)> onConnectionResolved,
        std::function<void(uint64_t, asio::ip::tcp::socket, std::string)> onFilesConnectionResolved)
        : m_asioContext(asioContext),
        m_socket(std::move(socket)),
        m_timeoutTimer(asioContext),
        m_id(id),
        m_onConnectError(std::move(errorCallback)),
        m_onConnectionResolved(std::move(onConnectionResolved)),
        m_onFilesConnectionResolved(std::move(onFilesConnectionResolved))
    {
        m_handshakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());
        m_handshakeIn = 0;
        m_handshakeCheckForFiles = 0;
        m_handshakeCheckForMessages = 0;
        m_loginLength = 0;

        m_handshakeCheckForMessages = scramble(m_handshakeOut);
        m_handshakeCheckForFiles = m_handshakeCheckForMessages;
        m_handshakeCheckForFiles++;

        writeValidation();
        startTimeout();
        readValidation();
    }

    ConnectionTypeResolver::~ConnectionTypeResolver() {}

    void ConnectionTypeResolver::startTimeout() {
        m_timeoutTimer.expires_after(std::chrono::seconds(30));

        m_timeoutTimer.async_wait([this](const std::error_code& ec) {
            if (!ec) {
                disconnect();
                m_onConnectError(asio::error::timed_out, m_id);
            }
        });
    }

    void ConnectionTypeResolver::cancelTimeout() {
        m_timeoutTimer.cancel();
    }

    uint64_t ConnectionTypeResolver::scramble(uint64_t inputNumber) {
        uint64_t out = inputNumber ^ 0xDEADBEEFC;
        out = (out & 0xF0F0F0F0F) >> 4 | (out & 0x0F0F0F0F0F) << 4;
        return out ^ 0xC0DEFACE12345678;
    }

    void ConnectionTypeResolver::writeValidation() {
        asio::async_write(m_socket, asio::buffer(&m_handshakeOut, sizeof(uint64_t)),
            [this](std::error_code ec, std::size_t length) {
                if (ec) {
                    cancelTimeout();
                    disconnect();
                    m_onConnectError(ec, m_id);
                }
            });
    }

    void ConnectionTypeResolver::readValidation() {
        asio::async_read(m_socket, asio::buffer(&m_handshakeIn, sizeof(uint64_t)),
            [this](std::error_code ec, std::size_t length) {
                if (ec) {
                    cancelTimeout();
                    disconnect();
                    m_onConnectError(ec, m_id);
                }
                else {
                    if (m_handshakeIn == m_handshakeCheckForFiles) {
                        readLoginForBind();
                    }
                    else if (m_handshakeIn == m_handshakeCheckForMessages) {
                        completeMessagesSocketValidation();
                    }
                    else {
                        cancelTimeout();
                        disconnect();
                    }
                }
            });
    }

    void ConnectionTypeResolver::readLoginForBind() {
        asio::async_read(m_socket, asio::buffer(&m_loginLength, sizeof(uint32_t)),
            [this](std::error_code ec, std::size_t length) {
                if (ec) {
                    disconnect();
                    m_onConnectError(ec, m_id);
                }
                else {
                    m_login.resize(m_loginLength);
                    asio::async_read(m_socket, asio::buffer(&m_login[0], m_loginLength),
                        [this](std::error_code ec, std::size_t length) {
                            if (ec) {
                                cancelTimeout();
                                disconnect();
                                m_onConnectError(ec, m_id);
                            }
                            else {
                                completeFilesSocketValidation();
                            }
                        });
                }
            });
    }

    void ConnectionTypeResolver::completeFilesSocketValidation() {
        cancelTimeout();
        std::cout << "Files Client Validated. " << "\n";
        m_onFilesConnectionResolved(m_id, std::move(m_socket), m_login);
    }

    void ConnectionTypeResolver::completeMessagesSocketValidation() {
        cancelTimeout();
        std::cout << "Messages Client Validated." << "\n";
        m_onConnectionResolved(m_id, std::move(m_socket));
    }

    void ConnectionTypeResolver::disconnect() {
        if (m_socket.is_open()) {
            m_socket.close();
        }
    }
}