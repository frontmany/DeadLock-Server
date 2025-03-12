#pragma once

#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <asio.hpp>

#include"database.h"
#include "user.h"
#include "sender.h"

class Server {
public:
    Server();
    void init(const std::string& ipAddress, int port);
    void run();

private:
    void acceptConnections();
    void onAccept(asio::ip::tcp::socket* socket);
    void onDisconnect(asio::ip::tcp::socket* socket);

    void sendResponse(asio::ip::tcp::socket* socket, std::string response);
    void startAsyncRead(asio::ip::tcp::socket* socket);
    void handleRead(const asio::error_code& ec, std::size_t bytes_transferred,
        asio::ip::tcp::socket* socket, std::shared_ptr<asio::streambuf> buffer);
    void returnUserInfo(asio::ip::tcp::socket* socket, std::string packet);

    void authorizeUser(asio::ip::tcp::socket* socket, std::string packet);
    void registerUser(asio::ip::tcp::socket* socket,std::string packet);
    void createChat(asio::ip::tcp::socket* socket, std::string packet);
    void updateUserInfo(asio::ip::tcp::socket* socket, std::string packet);
    void broadcastUserInfo(asio::ip::tcp::socket* socket, std::string packet);

    void handleBroadcast(asio::ip::tcp::socket* socket, std::string packet);
    void handleGet(asio::ip::tcp::socket* socket, std::string packet);
    void handleRpl(asio::ip::tcp::socket* socket, std::string packet);

    std::string rebuildRemainingStringFromIss(std::istringstream& iss);

private:
    SendStringsGenerator                m_sender;
    Database                            m_db;
    asio::io_context                    m_io_context;
    asio::ip::tcp::acceptor             m_acceptor;
    std::string                         m_ipAddress;
    int                                 m_port;
    std::mutex                          m_mtx;
    asio::thread_pool                   m_thread_pool;

    std::map<std::string, User*>  m_map_online_users;
    std::vector<std::thread>      m_vec_threads;
};