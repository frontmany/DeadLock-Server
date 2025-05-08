#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <cstring>
#include <chrono>
#include <memory>
#include <ctime>

#include "photo.h"


namespace net {
    template <typename T>
    class connection;
}

enum class QueryType : uint32_t;

typedef std::shared_ptr<net::connection<QueryType>> connectionT;

class User {
public:
    User() = default;

    User(const std::string& login, const std::string& passwordHash, const std::string& name, bool isHasPhoto, Photo photo, connectionT connection)
        : m_login(login), m_password_hash(passwordHash),
        m_name(name), m_is_has_photo(isHasPhoto), m_photo(photo), m_connection(connection) {}

    User(const std::string& login, const std::string& passwordHash, const std::string& name, bool isHasPhoto, Photo photo)
        : m_login(login), m_password_hash(passwordHash),
        m_name(name), m_is_has_photo(isHasPhoto), m_photo(photo) {}


    ~User() = default;


    connectionT getConnection() const { return m_connection; }
    void setConnection(connectionT connection) { m_connection = connection; }

    const std::string& getLogin() const { return m_login; }
    void setLogin(const std::string& login) { m_login = login; }

    const std::string& getPassword() const { return m_password_hash; }
    void setPassword(const std::string& passwordHash) { m_password_hash = passwordHash; }

    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    const Photo& getPhoto() const { return m_photo; }
    void setPhoto(const Photo& photo) { m_photo = photo; }

    const bool getIsHasPhoto() const { return m_is_has_photo; }
    void setIsHasPhoto(bool isHasPhoto) { m_is_has_photo = isHasPhoto; }

    const std::string& getLastSeen() const { return m_last_seen; }
    void setLastSeen(const std::string& lastSeen) { m_last_seen = lastSeen; }

    void setLastSeenToNow();
    void setLastSeenToOnline();

private:
    bool                                    m_is_has_photo = false;
    std::string			                    m_last_seen;
    std::string			                    m_name;
    std::string			                    m_login;
    std::string			                    m_password_hash;
    Photo			                        m_photo;
    connectionT                             m_connection;
};

