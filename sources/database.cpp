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
        "LOGIN_HASH      TEXT PRIMARY KEY  NOT NULL,"
        "LOGIN           TEXT              ,"
        "NAME            TEXT              ,"
        "PASSWORD_HASH   TEXT              NOT NULL,"
        "ENCRYPTION_PART TEXT              NOT NULL,"
        "LAST_SEEN       TEXT              NOT NULL,"
        "PUBLIC_KEY      TEXT              ,"
        "IS_HAS_PHOTO    INTEGER           NOT NULL,"
        "PHOTO_PATH      TEXT              NOT NULL,"
        "PHOTO_SIZE      INTEGER           NOT NULL);";

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

void Database::addUser(const std::string& loginHash,
    const std::string& passwordHash,
    const std::string& encryptionPart,
    const std::string& lastSeen) {
    const char* sql = "INSERT INTO USER ("
        "LOGIN_HASH, PASSWORD_HASH, ENCRYPTION_PART, LAST_SEEN, "
        "IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE, "
        "LOGIN, NAME, PUBLIC_KEY) "
        "VALUES (?, ?, ?, ?, 0, '', 0, '', '', '');";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, loginHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, encryptionPart.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, lastSeen.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
    }
    else {
        std::cout << "User added successfully" << std::endl;
    }

    sqlite3_finalize(stmt);
}

User* Database::getUser(const std::string& loginHash) {
    sqlite3_stmt* stmt = nullptr;
    std::string sql = "SELECT LOGIN_HASH, LOGIN, NAME, PASSWORD_HASH, LAST_SEEN, PUBLIC_KEY, "
        "IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE FROM USER WHERE LOGIN_HASH = ?";

    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return nullptr;
    }

    rc = sqlite3_bind_text(stmt, 1, loginHash.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to bind parameter: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(stmt);
        return nullptr;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string loginHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string login = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        std::string passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        std::string lastSeen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        std::string publicKey = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

        bool isHasPhoto = sqlite3_column_int(stmt, 6) == 1;
        std::string photoPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        int photoSize = sqlite3_column_int(stmt, 8);

        Photo photo(photoPath, photoSize);
        User* user = new User(login, loginHash, passwordHash, name, isHasPhoto, photo, crypto::deserializePublicKey(publicKey));
        user->setLastSeen(lastSeen);

        sqlite3_finalize(stmt);
        return user;
    }
    else {
        std::cout << "No user found with login hash: " << loginHash << std::endl;
        sqlite3_finalize(stmt);
        return nullptr;
    }
}

std::vector<std::string> Database::getUsersStatusesVec(const std::vector<std::string>& loginsVec, const std::map<std::string, User*>& mapOnlineUsers) {
    std::vector<std::string> statuses;
    for (const auto& login : loginsVec) {

        auto it = mapOnlineUsers.find(login);
        if (it != mapOnlineUsers.end()) {
            statuses.push_back("online"); 
            continue;
        }

        std::string statusFromDb = getUser(login)->getLastSeen(); 
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

    char* zErrMsg = nullptr;
    int rc;
    std::string sql = "SELECT PASSWORD_HASH FROM USER WHERE LOGIN_HASH = ?;";
    std::string storedHashedPassword;

    auto passwordCallback = [](void* data, int argc, char** argv, char** azColName) -> int {
        if (argc > 0 && argv[0]) {
            *reinterpret_cast<std::string*>(data) = argv[0];
        }
        return 0;
        };


    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, loginHash.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const char* passwordText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (passwordText) {
            storedHashedPassword = passwordText;
        }
    }
    else if (rc != SQLITE_DONE) {
        std::cerr << "SQL error: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);

    if (storedHashedPassword.empty()) {
        return false;
    }

    return passwordHash == storedHashedPassword;
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

std::vector<User*> Database::findUsers(const std::string& currentUserLoginHash, const std::string& searchText, std::vector<User*>& foundUsers) {
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

            
        Photo* photo = new Photo(photoPath);
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




void Database::updateUserLogin(const std::string& loginHash, const std::string& newLogin) {
    const std::string newLoginHash = crypto::calculateHash(newLogin);

    const char* sql = "UPDATE USER SET LOGIN = ?, LOGIN_HASH = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { newLogin, newLoginHash, loginHash });
}

void Database::updateUserName(const std::string& loginHash, const std::string& name) {
    const char* sql = "UPDATE USER SET NAME = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { name, loginHash });
}

void Database::updateUserPassword(const std::string& loginHash, const std::string& passwordHash) {
    const char* sql = "UPDATE USER SET PASSWORD_HASH = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { passwordHash, loginHash });
}

void Database::updateUserEncryptionPart(const std::string& loginHash, const std::string& encryptionPart) {
    const char* sql = "UPDATE USER SET ENCRYPTION_PART = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { encryptionPart, loginHash });
}

void Database::updateUserLastSeen(const std::string& loginHash, const std::string& lastSeen) {
    const char* sql = "UPDATE USER SET LAST_SEEN = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { lastSeen, loginHash });
}

void Database::updateUserPublicKey(const std::string& loginHash, const std::string& publicKey) {
    const char* sql = "UPDATE USER SET PUBLIC_KEY = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { publicKey, loginHash });
}

void Database::updateUserPhoto(const std::string& loginHash, const Photo& photo, size_t photoSize) {
    const char* sql = "UPDATE USER SET IS_HAS_PHOTO = ?, PHOTO_PATH = ? , PHOTO_SIZE = ? WHERE LOGIN_HASH = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, true);
    sqlite3_bind_text(stmt, 2, photo.getPhotoPath().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, photoSize);
    sqlite3_bind_text(stmt, 4, loginHash.c_str(), -1, SQLITE_STATIC);

    executeAndCheck(stmt, "photo");
    sqlite3_finalize(stmt);
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

void Database::executeAndCheck(sqlite3_stmt* stmt, const std::string& operation) {
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to update " << operation << ": " << sqlite3_errmsg(m_db) << std::endl;
    }
    else if (sqlite3_changes(m_db) == 0) {
        std::cerr << "User not found or data not changed" << std::endl;
    }
    else {
        std::cout << "Successfully updated " << operation << std::endl;
    }
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
