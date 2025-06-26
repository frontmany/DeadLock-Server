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

#include "rsa.h"


namespace net {
    template <typename T>
    class connection;

    template <typename T>
    class files_connection;
}

enum class QueryType : uint32_t;

typedef std::shared_ptr<net::connection<QueryType>> connectionT;
typedef std::shared_ptr<net::files_connection<QueryType>> files_connectionT;

class User {
public:
    User() = default;

    // from db
    User(const std::string& login, const std::string& loginHash, const std::string& passwordHash, const std::string& name, bool isHasPhoto, Photo photo, CryptoPP::RSA::PublicKey publicKey)
        : m_login(login), m_login_hash(loginHash), m_password_hash(passwordHash),
        m_name(name), m_is_has_photo(isHasPhoto), m_public_key(publicKey), m_photo(photo) {}


    // on registration
    User(const std::string& loginHash, const std::string& passwordHash, bool isHasPhoto, Photo photo, connectionT connection)
        : m_login_hash(loginHash), m_password_hash(passwordHash),
        m_is_has_photo(isHasPhoto), m_photo(photo), m_connection(connection) {}


    ~User() = default;


    connectionT getConnection() const { return m_connection; }
    void setConnection(connectionT connection) { m_connection = connection; }

    files_connectionT getFilesConnection() const { return m_files_connection; }
    void setFilesConnection(files_connectionT filesConnection) { m_files_connection = filesConnection; }

    const std::string& getLogin() const { return m_login; }
    void setLogin(const std::string& login) { m_login = login; }

    const std::string& getLoginHash() const { return m_login_hash; }
    void setLoginHash(const std::string& loginHash) { m_login_hash = loginHash; }

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

    const std::string& getEncryptionPart() const { return m_encryption_part; }
    void setEncryptionPart(const std::string& encryptionPart) { m_encryption_part = encryptionPart; }

    void setPublicKey(const CryptoPP::RSA::PublicKey& key);
    const CryptoPP::RSA::PublicKey& getPublicKey() const;

    void setLastSeenToNow();
    void setLastSeenToOnline();

private:
    bool                                    m_is_has_photo = false;
    std::string			                    m_last_seen;
    std::string			                    m_name;
    std::string			                    m_login;
    std::string			                    m_login_hash;
    std::string			                    m_password_hash;
    Photo			                        m_photo;
    connectionT                             m_connection;
    files_connectionT                       m_files_connection;
    CryptoPP::RSA::PublicKey                m_public_key;
    std::string			                    m_encryption_part;
};

