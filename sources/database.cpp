#include "database.h" 
#include "crypto.h" 
#include "user.h"  


void Database::init() {
    int rc = sqlite3_open("Database.db", &m_db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }
    std::cout << "Opened database successfully" << std::endl;

    const char* sql1 = "CREATE TABLE IF NOT EXISTS USER("
        "LOGIN_HASH      TEXT              NOT NULL,"
        "LOGIN           TEXT              ,"
        "NAME            TEXT              ,"
        "PASSWORD_HASH   TEXT              NOT NULL,"
        "ENCRYPTION_PART TEXT              NOT NULL,"
        "LAST_SEEN       TEXT              NOT NULL,"
        "PUBLIC_KEY      TEXT              ,"
        "IS_HAS_PHOTO    INTEGER           NOT NULL,"
        "PHOTO_PATH      TEXT              NOT NULL,"
        "PHOTO_SIZE      TEXT              NOT NULL);";

    char* zErrMsg = 0;
    rc = sqlite3_exec(m_db, sql1, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return;
    }
    std::cout << "Table USER created successfully" << std::endl;

    const char* sql2 = "CREATE TABLE IF NOT EXISTS COLLECTED_PACKETS("
        "LOGIN_HASH     TEXT              NOT NULL,"
        "PACKET         TEXT              NOT NULL,"
        "PACKET_TYPE    INTEGER           NOT NULL);";

    rc = sqlite3_exec(m_db, sql2, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return;
    }
    std::cout << "Table COLLECTED_PACKETS created successfully" << std::endl;
}

bool Database::addUser(const std::string& loginHash,
    const std::string& passwordHash,
    const std::string& encryptionPartEnc,
    const std::string& lastSeenEnc)
{
    if (!m_db) {
        std::cerr << "Database not initialized" << std::endl;
        return false;
    }

    const char* sql = "INSERT INTO USER ("
        "LOGIN_HASH, PASSWORD_HASH, ENCRYPTION_PART, LAST_SEEN, "
        "IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE, "
        "LOGIN, NAME, PUBLIC_KEY) "
        "VALUES (?, ?, ?, ?, 0, '', '', '', '', '');";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    // Bind parameters with error checking
    auto bind_param = [&](int idx, const std::string& value) -> bool {
        if (sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            std::cerr << "Failed to bind parameter " << idx << ": " << sqlite3_errmsg(m_db) << std::endl;
            return false;
        }
        return true;
        };

    if (!bind_param(1, loginHash) || !bind_param(2, passwordHash) ||
        !bind_param(3, encryptionPartEnc) || !bind_param(4, lastSeenEnc)) {
        sqlite3_finalize(stmt);
        return false;
    }

    bool success = false;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        std::cout << "User added successfully" << std::endl;
        success = true;
    }
    else {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
        if (rc == SQLITE_CONSTRAINT) {
            std::cerr << "Duplicate login_hash detected" << std::endl;
        }
    }

    sqlite3_finalize(stmt);
    return success;
}

User* Database::getUser(CryptoPP::RSA::PrivateKey privateKey, const std::string& loginHash) {
    if (!m_db) {
        std::cerr << "Database not initialized" << std::endl;
        return nullptr;
    }

    const std::string sql = "SELECT "
        "LOGIN_HASH, LOGIN, NAME, PASSWORD_HASH, "
        "ENCRYPTION_PART, LAST_SEEN, PUBLIC_KEY, "
        "IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE "
        "FROM USER WHERE LOGIN_HASH = ?";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return nullptr;
    }

    rc = sqlite3_bind_text(stmt, 1, loginHash.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to bind parameter: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(stmt);
        return nullptr;
    }

    User* user = nullptr;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        try {
            std::string dbLoginHash = safeColumnText(stmt, 0);
            std::string encryptedLogin = safeColumnText(stmt, 1);
            std::string encryptedName = safeColumnText(stmt, 2);
            std::string passwordHash = safeColumnText(stmt, 3);
            std::string encryptedEncryptionPart = safeColumnText(stmt, 4);
            std::string encryptedLastSeen = safeColumnText(stmt, 5);
            std::string publicKeyStr = safeColumnText(stmt, 6);

            bool isHasPhoto = sqlite3_column_int(stmt, 7) == 1;
            std::string encryptedPhotoPath = safeColumnText(stmt, 8);
            std::string encryptedPhotoSize = safeColumnText(stmt, 9);

            std::string login = crypto::RSADecrypt(privateKey, encryptedLogin);
            std::string name = crypto::RSADecrypt(privateKey, encryptedName);
            std::string encryptionPart = crypto::RSADecrypt(privateKey, encryptedEncryptionPart);
            std::string lastSeen = crypto::RSADecrypt(privateKey, encryptedLastSeen);
            std::string photoPath = encryptedPhotoPath.empty() ? "" : crypto::RSADecrypt(privateKey, encryptedPhotoPath);
            std::string photoSize = photoSize.empty() ? "" : crypto::RSADecrypt(privateKey, encryptedPhotoSize);

            user = new User(login, dbLoginHash, passwordHash, name, isHasPhoto, Photo(privateKey, photoPath));
            user->setEncryptionPart(encryptionPart);
            user->setLastSeen(lastSeen);

            if (!publicKeyStr.empty()) {
                user->setPublicKey(crypto::deserializePublicKey(publicKeyStr));
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing user data: " << e.what() << std::endl;
            delete user;
            user = nullptr;
        }
    }
    else if (rc == SQLITE_DONE) {
        std::cout << "No user found with login hash: " << loginHash << std::endl;
    }
    else {
        std::cerr << "SQL error: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return user;
}


std::string Database::safeColumnText(sqlite3_stmt* stmt, int column) {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
    return text ? text : "";
}

std::vector<std::string> Database::getUsersStatusesVec(CryptoPP::RSA::PrivateKey privateKey, const std::vector<std::string>& loginsVec, const std::map<std::string, User*>& mapOnlineUsers) {
    std::vector<std::string> statuses;
    for (const auto& login : loginsVec) {

        auto it = mapOnlineUsers.find(login);
        if (it != mapOnlineUsers.end()) {
            statuses.push_back("online"); 
            continue;
        }

        std::string statusFromDb = getUser(privateKey, login)->getLastSeen(); 
        if (!statusFromDb.empty()) {
            statuses.push_back(statusFromDb);
        }
        else {
            statuses.push_back("offline"); 
            std::cout << "User not found: " << login << std::endl; 
        }
    }
    return statuses;
}

bool Database::checkPassword(const std::string& loginHash, const std::string& passwordHash) {
    if (!m_db) {
        std::cerr << "db not initialized (err: from check password func)!" << std::endl;
        return false;
    }

    const char* sql = "SELECT PASSWORD_HASH FROM USER WHERE LOGIN_HASH = ?;";
    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    rc = sqlite3_bind_text(stmt, 1, loginHash.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        std::cerr << "Bind failed: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* passwordText = sqlite3_column_text(stmt, 0);
        if (!passwordText) {
            sqlite3_finalize(stmt);
            return false;
        }
        std::string storedHashedPassword(reinterpret_cast<const char*>(passwordText));
        sqlite3_finalize(stmt);
        return (passwordHash == storedHashedPassword);
    }
    else if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return false;
    }
    else {
        std::cerr << "SQL error during step: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }
}


void Database::collect(const std::string& loginHash, const std::string& packet, QueryType type) {
    const char* sql = "INSERT INTO COLLECTED_PACKETS (LOGIN_HASH, PACKET, PACKET_TYPE) "
        "VALUES (?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int rc;

    rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, loginHash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, packet.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, static_cast<int>(type)); 

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
    }
    else {
        std::cout << "Packet collected successfully" << std::endl;
    }

    sqlite3_finalize(stmt);
}

std::vector<std::pair<std::string, QueryType>> Database::getCollected(const std::string& loginHash) {
    const char* selectSql = "SELECT PACKET, PACKET_TYPE FROM COLLECTED_PACKETS WHERE LOGIN_HASH = ?;";
    const char* deleteSql = "DELETE FROM COLLECTED_PACKETS WHERE LOGIN_HASH = ? AND PACKET = ?;";
    sqlite3_stmt* selectStmt = nullptr;
    sqlite3_stmt* deleteStmt = nullptr;
    int rc;
    std::vector<std::pair<std::string, QueryType>> packets;

    rc = sqlite3_prepare_v2(m_db, selectSql, -1, &selectStmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare SELECT statement: " << sqlite3_errmsg(m_db) << std::endl;
        return packets;
    }

    rc = sqlite3_bind_text(selectStmt, 1, loginHash.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to bind login in SELECT statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(selectStmt);
        return packets;
    }

    while ((rc = sqlite3_step(selectStmt)) == SQLITE_ROW) {
        const char* packet = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 0));
        if (!packet) continue;

        
        QueryType type = static_cast<QueryType>(sqlite3_column_int(selectStmt, 1));

        packets.emplace_back(packet, type);

        rc = sqlite3_prepare_v2(m_db, deleteSql, -1, &deleteStmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare DELETE statement: " << sqlite3_errmsg(m_db) << std::endl;
            continue;
        }

        rc = sqlite3_bind_text(deleteStmt, 1, loginHash.c_str(), -1, SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to bind login in DELETE statement: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(deleteStmt);
            continue;
        }

        rc = sqlite3_bind_text(deleteStmt, 2, packet, -1, SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to bind packet in DELETE statement: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(deleteStmt);
            continue;
        }

        rc = sqlite3_step(deleteStmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to delete packet: " << sqlite3_errmsg(m_db) << std::endl;
        }

        sqlite3_finalize(deleteStmt);
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(selectStmt);

    return packets;
}

std::vector<User*> Database::findUsers(const CryptoPP::RSA::PrivateKey& privateKey, const std::string& currentUserLoginHash, const std::string& searchText, std::vector<User*>& foundUsers) {
    const char* sql =
        "SELECT LOGIN, NAME, PHOTO_PATH FROM USER "
        "WHERE (LOGIN LIKE ? OR NAME LIKE ?) "
        "AND LOGIN_HASH != ? "
        "LIMIT 20;";

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return foundUsers;
    }

    std::string searchPattern = "%" + searchText + "%";

    sqlite3_bind_text(stmt, 1, searchPattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, searchPattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, currentUserLoginHash.c_str(), -1, SQLITE_TRANSIENT);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        User* user = new User();

        user->setLogin(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        user->setName(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));

        const char* photoPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

            
        Photo* photo = new Photo(privateKey, photoPath);
        user->setPhoto(*photo);
        if (photo->getPhotoPath() != "") {
            user->setIsHasPhoto(true);
        }

        foundUsers.push_back(user);
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Search failed: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return foundUsers;
}




bool Database::updateUserLogin(const CryptoPP::RSA::PublicKey& publicKey, const std::string& loginHash, const std::string& newLogin) {
    sqlite3_stmt* stmt = nullptr;
    char* errMsg = nullptr;
    bool result = false;

    if (sqlite3_exec(m_db, "BEGIN TRANSACTION", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }

    try {
        const char* checkSql = "SELECT 1 FROM USER WHERE LOGIN_HASH = ? LIMIT 1";
        if (sqlite3_prepare_v2(m_db, checkSql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(m_db));
        }

        sqlite3_bind_text(stmt, 1, loginHash.c_str(), -1, SQLITE_TRANSIENT);
        bool exists = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
        stmt = nullptr;

        if (!exists) {
            throw std::runtime_error("User with loginHash " + loginHash + " not found");
        }

        const std::string newLoginHash = crypto::calculateHash(newLogin);
        const std::string encryptedLogin = crypto::RSAEncrypt(publicKey, newLogin);

        const char* updateSql = "UPDATE USER SET LOGIN = ?, LOGIN_HASH = ? WHERE LOGIN_HASH = ?";

        if (sqlite3_prepare_v2(m_db, updateSql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error(sqlite3_errmsg(m_db));
        }

        sqlite3_bind_text(stmt, 1, encryptedLogin.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, newLoginHash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, loginHash.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            throw std::runtime_error(sqlite3_errmsg(m_db));
        }
        sqlite3_finalize(stmt);
        stmt = nullptr;

        if (sqlite3_exec(m_db, "COMMIT", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            throw std::runtime_error(errMsg);
        }
        sqlite3_free(errMsg);
        errMsg = nullptr;

        result = true;
    }
    catch (const std::exception& e) {
        std::cerr << "Error updating user login: " << e.what() << std::endl;
        if (sqlite3_exec(m_db, "ROLLBACK", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "Failed to rollback transaction: " << errMsg << std::endl;
        }
        if (errMsg) {
            sqlite3_free(errMsg);
            errMsg = nullptr;
        }
        if (stmt) {
            sqlite3_finalize(stmt);
            stmt = nullptr;
        }
        result = false;
    }

    return result;
}


void Database::updateUserLoginOnly(const std::string& loginHash, const std::string& newLoginEnc) {
    const char* sql = "UPDATE USER SET LOGIN = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { newLoginEnc, loginHash });
}

void Database::updateUserName(const std::string& loginHash, const std::string& nameEnc) {
    const char* sql = "UPDATE USER SET NAME = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { nameEnc, loginHash });
}

void Database::updateUserPassword(const std::string& loginHash, const std::string& passwordHash) {
    const char* sql = "UPDATE USER SET PASSWORD_HASH = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { passwordHash, loginHash });
}

void Database::updateUserEncryptionPart(const std::string& loginHash, const std::string& encryptionPartEnc) {
    const char* sql = "UPDATE USER SET ENCRYPTION_PART = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { encryptionPartEnc, loginHash });
}

void Database::updateUserLastSeen(const std::string& loginHash, const std::string& lastSeenEnc) {
    const char* sql = "UPDATE USER SET LAST_SEEN = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { lastSeenEnc, loginHash });
}

void Database::updateUserPublicKey(const std::string& loginHash, const std::string& publicKeyEnc) {
    const char* sql = "UPDATE USER SET PUBLIC_KEY = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { publicKeyEnc, loginHash });
}

void Database::updateUserPhoto(CryptoPP::RSA::PublicKey publicKey, const std::string& loginHash, const Photo& photo, size_t photoSize) {
    const char* sql = "UPDATE USER SET IS_HAS_PHOTO = ?, PHOTO_PATH = ?, PHOTO_SIZE = ? WHERE LOGIN_HASH = ?";

    std::vector<std::string> params;
    params.push_back("1"); 
    params.push_back(crypto::RSAEncrypt(publicKey, photo.getPhotoPath()));
    params.push_back(crypto::RSAEncrypt(publicKey, std::to_string(photoSize)));
    params.push_back(loginHash);

    executeUpdate(sql, params);
}

void Database::executeUpdate(const char* sql, const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    for (size_t i = 0; i < params.size(); ++i) {
        sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(stmt);
}




std::string Database::friendsToString(const std::vector<std::string>& friends) {
    std::stringstream ss;
    for (size_t i = 0; i < friends.size(); ++i) {
        ss << friends[i];
        if (i < friends.size() - 1) {
            ss << ",";
        }
    }
    return ss.str();
}

std::vector<std::string> Database::stringToFriends(const std::string& friendsString) {
    std::vector<std::string> friends;
    std::stringstream ss(friendsString);
    std::string friendLogin;
    while (std::getline(ss, friendLogin, ',')) {
        friends.push_back(friendLogin);
    }
    return friends;
}

std::string Database::getCurrentDateTime() {
    std::time_t now = std::time(0);
    std::tm* ltm = std::localtime(&now);

    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << ltm->tm_year + 1900 << "-"
        << std::setw(2) << std::setfill('0') << ltm->tm_mon + 1 << "-"
        << std::setw(2) << std::setfill('0') << ltm->tm_mday << " "
        << std::setw(2) << std::setfill('0') << ltm->tm_hour << ":"
        << std::setw(2) << std::setfill('0') << ltm->tm_min << ":"
        << std::setw(2) << std::setfill('0') << ltm->tm_sec;

    return "last seen: " + ss.str();
}

bool Database::checkNewLogin(const std::string& newLoginHash) {
    if (newLoginHash.empty()) {
        std::cerr << "Login cannot be empty" << std::endl;
        return false;
    }

    const char* sql = "SELECT 1 FROM USER WHERE LOGIN_HASH = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, newLoginHash.c_str(), -1, SQLITE_STATIC);

    bool loginExists = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        loginExists = true;
    }

    sqlite3_finalize(stmt);


    return !loginExists;
}
