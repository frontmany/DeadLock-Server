#include "user.h"
#include "crypto.h"
#include "net_connection.h"
#include "net_filesConnection.h"

// from db
User::User(const std::string& login,
    const std::string& loginHash,
    const std::string& passwordHash,
    const std::string& name,
    bool isHasAvatar,
    Avatar* avatar)
    : m_login(login),
    m_is_files_connection_established(false),
    m_login_hash(loginHash),
    m_password_hash(passwordHash),
    m_name(name),
    m_is_has_avatar(isHasAvatar),
    m_avatar(avatar)
{
}

User::User(const std::string& loginHash,
    const std::string& passwordHash,
    bool isHasAvatar, Avatar* avatar,
    ConnectionPtr connection)
    : m_login_hash(loginHash),
    m_is_files_connection_established(false),
    m_password_hash(passwordHash),
    m_is_has_avatar(isHasAvatar),
    m_avatar(avatar),
    m_connection(connection) 
{
}

User::~User() 
{
    if (m_connection) {
        m_connection->close();
    }


    if (m_is_files_connection_established) {
        m_files_connection->close();
    }

    delete m_avatar;
}


void User::setLastSeenToNow() {
    auto now = std::chrono::system_clock::now();

    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");

    m_last_seen = "last seen " + oss.str(); // "last seen 14:30:00"
}

void User::setLastSeenToOnline() {
    m_last_seen = "online";
}

const CryptoPP::RSA::PublicKey& User::getPublicKey() const {
    if (!crypto::validatePublicKey(m_public_key)) {
        assert("Public key is not initialized or invalid");
    }

    return m_public_key;
}

void User::setPublicKey(const CryptoPP::RSA::PublicKey& key) {
    if (!crypto::validatePublicKey(key)) {
        assert("Invalid public key provided");
    }

    m_public_key = key;
}
