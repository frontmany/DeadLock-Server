#include "database.h" 
#include "hasher.h"
#include "user.h"  


void Database::init() {
    int rc = sqlite3_open("Database.db", &m_db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }
    std::cout << "Opened database successfully" << std::endl;

    const char* sql1 = "CREATE TABLE IF NOT EXISTS USER("
        "LOGIN          TEXT PRIMARY KEY  NOT NULL,"
        "NAME           TEXT              NOT NULL,"
        "PASSWORD_HASH  TEXT              NOT NULL,"
        "LAST_SEEN      TEXT              NOT NULL,"
        "IS_HAS_PHOTO   INTEGER           NOT NULL,"
        "PHOTO_PATH     TEXT              NOT NULL,"
        "PHOTO_SIZE     INTEGER           NOT NULL);";

    char* zErrMsg = 0;
    rc = sqlite3_exec(m_db, sql1, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return;
    }
    std::cout << "Table USER created successfully" << std::endl;

    const char* sql2 = "CREATE TABLE IF NOT EXISTS COLLECTED_PACKETS("
        "LOGIN          TEXT              NOT NULL,"
        "PACKET         TEXT              NOT NULL);";

    rc = sqlite3_exec(m_db, sql2, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
        return;
    }
    std::cout << "Table COLLECTED_PACKETS created successfully" << std::endl;
}

void Database::addUser(const std::string& login, const std::string& name, const std::string& lastSeen, const std::string& passwordHash) {
    const char* sql = "INSERT INTO USER (LOGIN, NAME, PASSWORD_HASH, LAST_SEEN, IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE) "
        "VALUES (?, ?, ?, ?, 0, '', 0);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, passwordHash.c_str(), -1, SQLITE_TRANSIENT);
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

User* Database::getUser(const std::string& login, asio::ip::tcp::socket* acceptSocket) {
    sqlite3_stmt* stmt = nullptr;
    std::string sql = "SELECT LOGIN, NAME, PASSWORD_HASH, LAST_SEEN, IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE FROM USER WHERE LOGIN = ?";

    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return nullptr;
    }

    rc = sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to bind parameter: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(stmt);
        return nullptr;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string login = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        std::string lastSeen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        bool isHasPhoto = sqlite3_column_int(stmt, 4) == 1;

        std::string photoPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        int photoSize = sqlite3_column_int(stmt, 6);
        Photo photo(photoPath, photoSize);

        User* user = nullptr;
        if (isHasPhoto) {
            user = new User(login, passwordHash, name, isHasPhoto, photo, acceptSocket);
            user->setLastSeen(lastSeen);
        }
        else {
            user = new User(login, passwordHash, name, isHasPhoto, Photo(), acceptSocket);
            user->setLastSeen(lastSeen);
        }

        sqlite3_finalize(stmt);
        return user;
    }
    else {
        std::cout << "No user found with login: " << login << std::endl;
        sqlite3_finalize(stmt);
        return nullptr;
    }
}

bool Database::checkIsNewLoginAvailable(const std::string& newLogin) {
    sqlite3_stmt* stmt = nullptr;

    std::string sql = "SELECT COUNT(*) FROM USER WHERE LOGIN = ?";

    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    rc = sqlite3_bind_text(stmt, 1, newLogin.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to bind parameter: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    bool isAvailable = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int count = sqlite3_column_int(stmt, 0);
        isAvailable = (count == 0);
    }

    sqlite3_finalize(stmt);
    return isAvailable;
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

bool Database::checkPassword(const std::string& login, const std::string& password) {
    if (!m_db) {
        std::cerr << "db not initialized (err: from check password func)!" << std::endl;
        return false;
    }

    char* zErrMsg = nullptr;
    int rc;
    std::string sql = "SELECT PASSWORD_HASH FROM USER WHERE LOGIN = ?;";
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

    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);

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

    return hash::verifyPassword(password, storedHashedPassword);
}

void Database::collect(const std::string& login, const std::string& packet) {
    const char* sql = "INSERT INTO COLLECTED_PACKETS (LOGIN, PACKET) "
        "VALUES (?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int rc;

    rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, login.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, packet.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
    }
    else {
        std::cout << "User  added successfully" << std::endl;
    }

    sqlite3_finalize(stmt);
}

std::vector<std::string> Database::getCollected(const std::string& login) {
    const char* selectSql = "SELECT PACKET FROM COLLECTED_PACKETS WHERE LOGIN = ?;";
    const char* deleteSql = "DELETE FROM COLLECTED_PACKETS WHERE LOGIN = ? AND PACKET = ?;";
    sqlite3_stmt* selectStmt = nullptr;
    sqlite3_stmt* deleteStmt = nullptr;
    int rc;
    std::vector<std::string> packets;

    // ���������� SELECT-�������
    rc = sqlite3_prepare_v2(m_db, selectSql, -1, &selectStmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare SELECT statement: " << sqlite3_errmsg(m_db) << std::endl;
        return packets; // ���������� ������ ������ � ������ ������
    }

    // �������� ��������� ��� SELECT-�������
    rc = sqlite3_bind_text(selectStmt, 1, login.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to bind login in SELECT statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(selectStmt);
        return packets;
    }

    // ������ ������
    while ((rc = sqlite3_step(selectStmt)) == SQLITE_ROW) {
        const char* packet = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 0));
        if (packet) {
            packets.emplace_back(packet);

            // ���������� DELETE-�������
            rc = sqlite3_prepare_v2(m_db, deleteSql, -1, &deleteStmt, nullptr);
            if (rc != SQLITE_OK) {
                std::cerr << "Failed to prepare DELETE statement: " << sqlite3_errmsg(m_db) << std::endl;
                continue; // ���������� ��������, ���� ������ �� ������� �����������
            }

            // �������� ���������� ��� DELETE-�������
            rc = sqlite3_bind_text(deleteStmt, 1, login.c_str(), -1, SQLITE_STATIC);
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

            // ���������� DELETE-�������
            rc = sqlite3_step(deleteStmt);
            if (rc != SQLITE_DONE) {
                std::cerr << "Failed to delete packet: " << sqlite3_errmsg(m_db) << std::endl;
            }

            sqlite3_finalize(deleteStmt);
        }
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(selectStmt);

    return packets;
}

void Database::updateUser(const std::string& oldLogin, const std::string& newLogin, const std::string& name, const std::string& password, bool isHasPhoto, Photo photo) {
    const char* sql = "UPDATE USER SET LOGIN = ?, NAME = ?, PASSWORD_HASH = ?, IS_HAS_PHOTO = ?, PHOTO_PATH = ?, PHOTO_SIZE = ? WHERE LOGIN = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    std::cout << "[DEBUG] stmt address: " << stmt << std::endl;

    const int hasPhotoInt = isHasPhoto ? 1 : 0;
    const std::string photoPath = isHasPhoto ? photo.getPhotoPath() : "";
    const int photoSize = isHasPhoto ? photo.getSize() : 0;
    const std::string hashedPassword = hash::hashPassword(password);

    sqlite3_bind_text(stmt, 1, newLogin.c_str(), -1, SQLITE_STATIC);  
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, hashedPassword.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, hasPhotoInt);
    sqlite3_bind_text(stmt, 5, photoPath.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, photoSize);
    sqlite3_bind_text(stmt, 7, oldLogin.c_str(), -1, SQLITE_STATIC); 

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
    }
    else if (sqlite3_changes(m_db) == 0) {
        std::cerr << "User with login '" << oldLogin << "' not found" << std::endl;
    }
    else {
        std::cout << "User updated successfully" << std::endl;
    }

    sqlite3_finalize(stmt);
}

void Database::updateUserStatus(const std::string& login, std::string lastSeen) {
    const char* sql = "UPDATE USER SET LAST_SEEN = ? WHERE LOGIN = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc;

    // ���������� SQL-�������
    rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, lastSeen.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, login.c_str(), -1, SQLITE_STATIC);

    // ���������� �������
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Execution failed: " << sqlite3_errmsg(m_db) << std::endl;
    }
    else {
        std::cout << "(on disconnect) User status updated successfully" << std::endl;
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