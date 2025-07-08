#include "user.h"
#include "crypto.h"

void User::setLastSeenToNow() {
    auto now = std::chrono::system_clock::now();

    std::time_t now_time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");

    m_last_seen = "last seen " + oss.str(); // например "last seen 14:30:00"
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
