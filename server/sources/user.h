#pragma once

#include<iostream>
#include<sstream>
#include<string>
#include<winsock2.h>
#include<ws2tcpip.h>
#include <fstream>
#include <cstring>
#include <chrono>
#include <memory>
#include <ctime>

#include "photo.h"
#include <asio.hpp>


class User {
public:
    User(const std::string& login, const std::string& passwordHash, const std::string& name, bool isHasPhoto, Photo photo, asio::ip::tcp::socket* sock)
        : m_login(login), m_password_hash(passwordHash),
        m_name(name), m_is_has_photo(m_is_has_photo), m_photo(photo), m_on_server_sock(sock) {}

    ~User() { 
        resetSocket();
        m_on_server_sock->close();
        delete m_on_server_sock; 
    
    }

    asio::ip::tcp::socket* getSocketOnServer() const { return m_on_server_sock; }
    void setSocketOnServer(asio::ip::tcp::socket* sock) { m_on_server_sock = sock; }

    const std::string& getLogin() const { return m_login; }
    void setLogin(const std::string& login) { m_login = login; }

    const std::string& getPassword() const { return m_password_hash; }
    void setPassword(const std::string& passwordHash) { m_password_hash = passwordHash; }

    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    const Photo& getPhoto() const { return m_photo; }
    void setPhoto(const Photo& photo) { m_photo = photo; m_is_has_photo = true; }

    const bool getIsHasPhoto() const { return m_is_has_photo; }

    const std::string& getLastSeen() const { return m_last_seen; }
    void setLastSeen(const std::string& lastSeen) { m_last_seen = lastSeen; }

    void resetSocket() {
        if ((*m_on_server_sock).is_open()) {
            asio::error_code ec;
            (*m_on_server_sock).close(ec);
            if (ec) {
                std::cerr << "Ошибка закрытия сокета: " << ec.message() << std::endl;
            }
        }
    }

    void setLastSeenToNow();
    void setLastSeenToOnline();

private:
    bool                                    m_is_has_photo;
    std::string			                    m_last_seen;
    std::string			                    m_name;
    std::string			                    m_login;
    std::string			                    m_password_hash;
    Photo			                        m_photo;

    asio::ip::tcp::socket*                  m_on_server_sock;
};

