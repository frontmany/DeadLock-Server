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
#include "queryType.h"

#include "rsa.h"


namespace net {
    class Connection;
    class FilesConnection;
}

typedef std::shared_ptr<net::Connection> ConnectionPtr;
typedef std::shared_ptr<net::FilesConnection> FilesConnectionPtr;

class User {
public:
    User() = default;

    // from db
    User(const std::string& login,
        const std::string& loginHash,
        const std::string& passwordHash,
        const std::string& name,
        bool isHasPhoto,
        Photo photo
    );


    // on registration
    User(const std::string& loginHash,
        const std::string& passwordHash,
        bool isHasPhoto,
        Photo photo,
        ConnectionPtr connection
    );


    ~User();

    ConnectionPtr getConnection() const { return m_connection; }
    void setConnection(ConnectionPtr connection) { m_connection = connection; }

    FilesConnectionPtr getFilesConnection() const { return m_files_connection; }
    void setFilesConnection(FilesConnectionPtr filesConnection) { m_files_connection = filesConnection; m_is_files_connection_established = true; }

    const std::string& getLogin() const { return m_login; }
    void setLogin(const std::string& login) { m_login = login; }

    const std::string& getLoginHash() const { return m_login_hash; }
    void setLoginHash(const std::string& loginHash) { m_login_hash = loginHash; }

    const std::string& getPasswordHash() const { return m_password_hash; }
    void setPasswordHash(const std::string& passwordHash) { m_password_hash = passwordHash; }

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
    bool                                    m_is_has_photo;
    bool                                    m_is_files_connection_established;
    std::string			                    m_last_seen;
    std::string			                    m_name;
    std::string			                    m_login;
    std::string			                    m_login_hash;
    std::string			                    m_password_hash;
    Photo			                        m_photo;
    ConnectionPtr                           m_connection;
    FilesConnectionPtr                      m_files_connection;
    CryptoPP::RSA::PublicKey                m_public_key;
    std::string			                    m_encryption_part;
};

