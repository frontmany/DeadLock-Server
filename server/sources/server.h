#pragma once

#include <iostream>
#include <sstream>
#include <thread>
#include <unordered_map>
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
    void onAccept(std::shared_ptr<asio::ip::tcp::socket> socket);
    void sendResponse(std::shared_ptr<asio::ip::tcp::socket> socket, std::string response);
    void startAsyncRead(std::shared_ptr<asio::ip::tcp::socket> socket);
    void handleRead(const asio::error_code& ec, std::size_t bytes_transferred,
        std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<asio::streambuf> buffer);
    void returnUserInfo(std::shared_ptr<asio::ip::tcp::socket> socket, std::string packet);

    void authorizeUser(std::shared_ptr<asio::ip::tcp::socket> acceptSocket, std::string packet);
    void registerUser(std::shared_ptr<asio::ip::tcp::socket> acceptSocket,std::string packet);
    void createChat(std::shared_ptr<asio::ip::tcp::socket> acceptSocket, std::string packet);
    void updateUserInfo(std::shared_ptr<asio::ip::tcp::socket> acceptSocket, std::string packet);
    void broadcastUserInfo(std::shared_ptr<asio::ip::tcp::socket> acceptSocket, std::string packet);

    void handleBroadcast(std::shared_ptr<asio::ip::tcp::socket> socket, std::string packet);
    void handleGet(std::shared_ptr<asio::ip::tcp::socket> socket, std::string packet);
    void handleRpl(std::shared_ptr<asio::ip::tcp::socket> socket, std::string packet);


    std::string rebuildRemainingStringFromIss(std::istringstream& iss);

    /*
    void redirectMessage(rpl::Message* message);
    void sendStatusToFriends(asio::ip::tcp::socket& socket, bool status);
    void updateUserInfo(asio::ip::tcp::socket& socket, rcv::UpdateUserInfoPacket& packet);
    void onDisconnect(asio::ip::tcp::socket& socket);
    */

private:
    SendStringsGenerator                m_sender;
    Database                            m_db;
    asio::io_context                    m_io_context;
    asio::ip::tcp::acceptor             m_acceptor;
    std::string                         m_ipAddress;
    int                                 m_port;
    std::mutex                          m_mtx;
    asio::thread_pool                   m_thread_pool;

    std::unordered_map<std::string, User*>  m_map_online_users;
    std::vector<std::thread>                m_vec_threads;

};