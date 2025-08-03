#include "database.h" 
#include "crypto.h" 
#include "user.h"  
#include "server.h"  
#include "packetsBuilder.h"  



void Database::init() {
    int rc = sqlite3_open("Database.db", &m_db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }
    std::cout << "Opened database successfully" << std::endl;

    rc = sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to enable foreign keys: " << sqlite3_errmsg(m_db) << std::endl;
    }

    const char* userTableSQL = "CREATE TABLE IF NOT EXISTS USER("
        "LOGIN_HASH      TEXT    NOT NULL PRIMARY KEY,"
        "LOGIN           TEXT    UNIQUE,"
        "NAME            TEXT,"
        "PASSWORD_HASH   TEXT    NOT NULL,"
        "ENCRYPTION_PART TEXT    NOT NULL,"
        "LAST_SEEN       TEXT    NOT NULL,"
        "PUBLIC_KEY      TEXT,"
        "IS_HAS_PHOTO    INTEGER NOT NULL DEFAULT 0,"
        "PHOTO_PATH      TEXT    NOT NULL DEFAULT '',"
        "PHOTO_SIZE      INTEGER NOT NULL DEFAULT 0);";

    executeSQL(userTableSQL, "USER");

    const char* packetsTableSQL = "CREATE TABLE IF NOT EXISTS COLLECTED_PACKETS("
        "id                INTEGER PRIMARY KEY AUTOINCREMENT,"
        "LOGIN_HASH_TO     TEXT    NOT NULL,"
        "LOGIN_HASH_FROM   TEXT    NOT NULL,"
        "PACKET            TEXT    NOT NULL,"
        "PACKET_TYPE       INTEGER NOT NULL);";

    executeSQL(packetsTableSQL, "COLLECTED_PACKETS");

    const char* blobsTableSQL = "CREATE TABLE IF NOT EXISTS BLOBS("
        "BLOB_UID              TEXT    NOT NULL PRIMARY KEY,"
        "LOGIN_HASH_TO         TEXT    NOT NULL,"
        "LOGIN_HASH_FROM       TEXT    NOT NULL,"
        "FILES_COUNT_IN_BLOB   INTEGER NOT NULL CHECK(FILES_COUNT_IN_BLOB > 0),"
        "FILES_RECEIVED        INTEGER NOT NULL DEFAULT 0,"
        "FILES_SENT            INTEGER NOT NULL DEFAULT 0,"
        "CHECK (FILES_RECEIVED <= FILES_COUNT_IN_BLOB));";

    executeSQL(blobsTableSQL, "BLOBS");

    const char* blobFilesTableSQL = "CREATE TABLE IF NOT EXISTS BLOB_FILES("
        "BLOB_UID      TEXT    NOT NULL,"
        "FILE_PACKET   TEXT    NOT NULL,"
        "FILE_ID       TEXT    NOT NULL);";

    executeSQL(blobFilesTableSQL, "BLOB_FILES");


    const char* avatarPacketsTableSQL = "CREATE TABLE IF NOT EXISTS AVATAR_PACKETS("
        "AVATAR_PATH            TEXT    NOT NULL,"
        "AVATAR_OWNER_LOGINHASH TEXT    NOT NULL,"
        "PHOTO_SIZE             INTEGER NOT NULL,"
        "LOGINHASH_TO           TEXT    NOT NULL,"
        "PRIMARY KEY (AVATAR_OWNER_LOGINHASH, LOGINHASH_TO));";

    executeSQL(avatarPacketsTableSQL, "AVATAR_PACKETS");
}

bool Database::addAvatarPacketIfNotExists(const std::string& avatarPath,
    const std::string& ownerLoginHash,
    const std::string& loginHashTo,
    uint32_t avatarSize)
{
    const char* sql = "INSERT OR IGNORE INTO AVATAR_PACKETS "
        "(AVATAR_PATH, AVATAR_OWNER_LOGINHASH, PHOTO_SIZE, LOGINHASH_TO) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, avatarPath.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ownerLoginHash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, avatarSize);
    sqlite3_bind_text(stmt, 4, loginHashTo.c_str(), -1, SQLITE_TRANSIENT);

    bool result = true;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to insert avatar packet: " << sqlite3_errmsg(m_db) << std::endl;
        result = false;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::tuple<std::string, uint32_t, std::string>> Database::getAvatarPacketsByReceiver(
    const std::string& loginHashTo)
{
    const char* sql = "SELECT AVATAR_PATH, PHOTO_SIZE, AVATAR_OWNER_LOGINHASH FROM AVATAR_PACKETS "
        "WHERE LOGINHASH_TO = ?;";

    sqlite3_stmt* stmt = nullptr;
    std::vector<std::tuple<std::string, uint32_t, std::string>> results;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return results;
    }

    sqlite3_bind_text(stmt, 1, loginHashTo.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* path = sqlite3_column_text(stmt, 0);
        uint32_t size = sqlite3_column_int(stmt, 1);
        const unsigned char* owner = sqlite3_column_text(stmt, 2);

        if (path && owner) {
            results.emplace_back(
                reinterpret_cast<const char*>(path),
                size,
                reinterpret_cast<const char*>(owner)
            );
        }
    }

    sqlite3_finalize(stmt);
    return results;
}

bool Database::removeAvatarPacketsByReceiver(const std::string& loginHashTo) {
    const char* sql = "DELETE FROM AVATAR_PACKETS WHERE LOGINHASH_TO = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, loginHashTo.c_str(), -1, SQLITE_TRANSIENT);

    bool result = true;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to delete avatar packets: " << sqlite3_errmsg(m_db) << std::endl;
        result = false;
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<std::tuple<std::string, uint32_t, std::string>> Database::getAndRemoveAvatarPacketsByReceiver(
    const std::string& loginHashTo)
{
    if (sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(m_db) << std::endl;
        return {};
    }

    auto packets = getAvatarPacketsByReceiver(loginHashTo);

    const char* deleteSql = "DELETE FROM AVATAR_PACKETS WHERE LOGINHASH_TO = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, deleteSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare delete statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return {};
    }

    sqlite3_bind_text(stmt, 1, loginHashTo.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Failed to delete avatar packets: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(stmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return {};
    }

    sqlite3_finalize(stmt);

    if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return {};
    }

    return packets;
}











bool Database::addBlob(const std::string& blobUid,
    const std::string& loginHashTo,
    const std::string& loginHashFrom,
    int filesCountInBlob) 
{
    const char* sql = "INSERT INTO BLOBS(BLOB_UID, LOGIN_HASH_TO, LOGIN_HASH_FROM, FILES_COUNT_IN_BLOB) "
        "VALUES(?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "SQL error: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, loginHashTo.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, loginHashFrom.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, filesCountInBlob);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;

    if (!result) {
        std::cerr << "Failed to add blob: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return result;
}

bool Database::removeBlob(const std::string& blobUid) {
    char* errMsg = nullptr;
    if (sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << (errMsg ? errMsg : "") << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return false;
    }

    {
        const char* deleteFilesSql = "DELETE FROM BLOB_FILES WHERE BLOB_UID = ?;";
        sqlite3_stmt* deleteFilesStmt = nullptr;
        if (sqlite3_prepare_v2(m_db, deleteFilesSql, -1, &deleteFilesStmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare DELETE statement for blob files with UID: " << blobUid
                << ". Error: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        if (sqlite3_bind_text(deleteFilesStmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            std::cerr << "Failed to bind blob UID to delete files statement: " << blobUid
                << ". Error: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(deleteFilesStmt);
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        int rc = sqlite3_step(deleteFilesStmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to delete blob files with UID: " << blobUid
                << ". Error: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(deleteFilesStmt);
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        sqlite3_finalize(deleteFilesStmt);
    }

    {
        const char* deleteBlobSql = "DELETE FROM BLOBS WHERE BLOB_UID = ?;";
        sqlite3_stmt* deleteBlobStmt = nullptr;
        if (sqlite3_prepare_v2(m_db, deleteBlobSql, -1, &deleteBlobStmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare DELETE statement for blob UID: " << blobUid
                << ". Error: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        if (sqlite3_bind_text(deleteBlobStmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            std::cerr << "Failed to bind blob UID to delete blob statement: " << blobUid
                << ". Error: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(deleteBlobStmt);
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        int rc = sqlite3_step(deleteBlobStmt);
        bool result = (rc == SQLITE_DONE);

        if (!result) {
            std::cerr << "Failed to delete blob with UID: " << blobUid
                << ". Error: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(deleteBlobStmt);
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        else {
            int changes = sqlite3_changes(m_db);
            if (changes == 0) {
                std::cout << "No blob found with UID: " << blobUid << ", nothing deleted." << std::endl;
                result = false;
            }
            else {
                std::cout << "Successfully deleted blob with UID: " << blobUid << std::endl;
            }
        }

        sqlite3_finalize(deleteBlobStmt);

        if (!result) {
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
    }

    if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to commit transaction: " << (errMsg ? errMsg : "") << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}


bool Database::addFileToBlob(const std::string& blobUid, const std::string& fileId, const std::string& filePacket) {
    const char* sql = "INSERT INTO BLOB_FILES(BLOB_UID, FILE_ID, FILE_PACKET) VALUES(?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, fileId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, filePacket.c_str(), -1, SQLITE_TRANSIENT);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    if (!result) {
        std::cerr << "Failed to add file to blob: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return result;
}


bool Database::incrementFilesReceivedCounter(const std::string& blobUid) {
    const char* sql = "UPDATE BLOBS SET FILES_RECEIVED = FILES_RECEIVED + 1 WHERE BLOB_UID = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool Database::incrementFilesSentCounter(const std::string& blobUid) {
    const char* sql = "UPDATE BLOBS SET FILES_SENT = FILES_SENT + 1 WHERE BLOB_UID = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT);

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return result;
}

bool Database::isBlobExists(const std::string& blobUid) {
    const char* sql = "SELECT 1 FROM BLOBS WHERE BLOB_UID = ? LIMIT 1;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT);

    bool exists = (sqlite3_step(stmt) == SQLITE_ROW);

    sqlite3_finalize(stmt);
    return exists;
}

Blob Database::getBlob(const std::string& blobUid) {
    Blob blob;

    const char* sql = "SELECT BLOB_UID, LOGIN_HASH_TO, LOGIN_HASH_FROM, FILES_COUNT_IN_BLOB, "
        "FILES_RECEIVED, FILES_SENT FROM BLOBS WHERE BLOB_UID = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement (main blob info): "
            << sqlite3_errmsg(m_db) << std::endl;
        return blob;
    }

    sqlite3_bind_text(stmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        blob.blobUID = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        blob.receiverLoginHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        blob.senderLoginHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        blob.filesCountInBlob = sqlite3_column_int(stmt, 3);
        blob.filesReceived = sqlite3_column_int(stmt, 4);
        blob.filesSent = sqlite3_column_int(stmt, 5);
    }
    sqlite3_finalize(stmt);

    const char* packetsSql = "SELECT FILE_PACKET FROM BLOB_FILES WHERE BLOB_UID = ?;";
    if (sqlite3_prepare_v2(m_db, packetsSql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement (file packets): "
            << sqlite3_errmsg(m_db) << std::endl;
        return blob;
    }

    sqlite3_bind_text(stmt, 1, blobUid.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* filePacket = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (filePacket != nullptr) {
            blob.filePacketsVec.emplace_back(filePacket);
        }
    }
    sqlite3_finalize(stmt);

    return blob;
}

std::vector<Blob> Database::getBlobsByLoginHashTo(const std::string& loginHashTo) {
    std::vector<Blob> blobs;

    const char* mainSql = "SELECT BLOB_UID, LOGIN_HASH_FROM, FILES_COUNT_IN_BLOB, "
        "FILES_RECEIVED, FILES_SENT FROM BLOBS "
        "WHERE LOGIN_HASH_TO = ?;";
    sqlite3_stmt* mainStmt;

    if (sqlite3_prepare_v2(m_db, mainSql, -1, &mainStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare main query: " << sqlite3_errmsg(m_db) << std::endl;
        return blobs;
    }

    sqlite3_bind_text(mainStmt, 1, loginHashTo.c_str(), -1, SQLITE_TRANSIENT);

    const char* filesSql = "SELECT FILE_PACKET FROM BLOB_FILES WHERE BLOB_UID = ?;";
    sqlite3_stmt* filesStmt;

    if (sqlite3_prepare_v2(m_db, filesSql, -1, &filesStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare files query: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(mainStmt);
        return blobs;
    }

    while (sqlite3_step(mainStmt) == SQLITE_ROW) {
        Blob blob;
        blob.receiverLoginHash = loginHashTo;
        blob.blobUID = reinterpret_cast<const char*>(sqlite3_column_text(mainStmt, 0));
        blob.senderLoginHash = reinterpret_cast<const char*>(sqlite3_column_text(mainStmt, 1));
        blob.filesCountInBlob = sqlite3_column_int(mainStmt, 2);
        blob.filesReceived = sqlite3_column_int(mainStmt, 3);
        blob.filesSent = sqlite3_column_int(mainStmt, 4);

        sqlite3_bind_text(filesStmt, 1, blob.blobUID.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(filesStmt) == SQLITE_ROW) {
            blob.filePacketsVec.push_back(
                reinterpret_cast<const char*>(sqlite3_column_text(filesStmt, 0))
            );
        }

        sqlite3_reset(filesStmt);
        sqlite3_clear_bindings(filesStmt);

        blobs.push_back(blob);
    }

    sqlite3_finalize(mainStmt);
    sqlite3_finalize(filesStmt);

    return blobs;
}

std::vector<Blob> Database::getBlobsByLoginHashFrom(const std::string& loginHashFrom) {
    std::vector<Blob> blobs;

    const char* mainSql = "SELECT BLOB_UID, LOGIN_HASH_TO, FILES_COUNT_IN_BLOB, "
        "FILES_RECEIVED, FILES_SENT FROM BLOBS "
        "WHERE LOGIN_HASH_FROM = ?;";
    sqlite3_stmt* mainStmt;

    if (sqlite3_prepare_v2(m_db, mainSql, -1, &mainStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare main query: " << sqlite3_errmsg(m_db) << std::endl;
        return blobs;
    }

    sqlite3_bind_text(mainStmt, 1, loginHashFrom.c_str(), -1, SQLITE_TRANSIENT);

    const char* filesSql = "SELECT FILE_PACKET FROM BLOB_FILES WHERE BLOB_UID = ?;";
    sqlite3_stmt* filesStmt;

    if (sqlite3_prepare_v2(m_db, filesSql, -1, &filesStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare files query: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(mainStmt);
        return blobs;
    }

    while (sqlite3_step(mainStmt) == SQLITE_ROW) {
        Blob blob;
        blob.senderLoginHash = loginHashFrom;
        blob.blobUID = reinterpret_cast<const char*>(sqlite3_column_text(mainStmt, 0));
        blob.receiverLoginHash = reinterpret_cast<const char*>(sqlite3_column_text(mainStmt, 1));
        blob.filesCountInBlob = sqlite3_column_int(mainStmt, 2);
        blob.filesReceived = sqlite3_column_int(mainStmt, 3);
        blob.filesSent = sqlite3_column_int(mainStmt, 4);

        sqlite3_bind_text(filesStmt, 1, blob.blobUID.c_str(), -1, SQLITE_TRANSIENT);

        while (sqlite3_step(filesStmt) == SQLITE_ROW) {
            blob.filePacketsVec.push_back(
                reinterpret_cast<const char*>(sqlite3_column_text(filesStmt, 0))
            );
        }

        sqlite3_reset(filesStmt);
        sqlite3_clear_bindings(filesStmt);

        blobs.push_back(blob);
    }

    sqlite3_finalize(mainStmt);
    sqlite3_finalize(filesStmt);

    return blobs;
}

bool Database::replaceAllBlobs(const std::string& loginHashFrom, const std::vector<Blob>& newBlobs) {
    if (sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    const char* selectBlobUIDsSql = "SELECT BLOB_UID FROM BLOBS WHERE LOGIN_HASH_FROM = ?;";
    sqlite3_stmt* selectStmt = nullptr;
    if (sqlite3_prepare_v2(m_db, selectBlobUIDsSql, -1, &selectStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare select statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (sqlite3_bind_text(selectStmt, 1, loginHashFrom.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        std::cerr << "Failed to bind loginHashFrom: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(selectStmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    const char* deleteFileSql = "DELETE FROM BLOB_FILES WHERE BLOB_UID = ?;";
    sqlite3_stmt* deleteFileStmt = nullptr;
    if (sqlite3_prepare_v2(m_db, deleteFileSql, -1, &deleteFileStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare delete file statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(selectStmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    while (sqlite3_step(selectStmt) == SQLITE_ROW) {
        const unsigned char* blobUID = sqlite3_column_text(selectStmt, 0);
        if (blobUID) {
            if (sqlite3_bind_text(deleteFileStmt, 1, reinterpret_cast<const char*>(blobUID), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                std::cerr << "Failed to bind blobUID for file deletion: " << sqlite3_errmsg(m_db) << std::endl;
                sqlite3_finalize(selectStmt);
                sqlite3_finalize(deleteFileStmt);
                sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
                return false;
            }

            if (sqlite3_step(deleteFileStmt) != SQLITE_DONE) {
                std::cerr << "Failed to delete blob files: " << sqlite3_errmsg(m_db) << std::endl;
                sqlite3_finalize(selectStmt);
                sqlite3_finalize(deleteFileStmt);
                sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
                return false;
            }

            sqlite3_reset(deleteFileStmt);
        }
    }
    sqlite3_finalize(selectStmt);
    sqlite3_finalize(deleteFileStmt);

    const char* deleteBlobsSql = "DELETE FROM BLOBS WHERE LOGIN_HASH_FROM = ?;";
    sqlite3_stmt* deleteBlobsStmt = nullptr;
    if (sqlite3_prepare_v2(m_db, deleteBlobsSql, -1, &deleteBlobsStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare delete blobs statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (sqlite3_bind_text(deleteBlobsStmt, 1, loginHashFrom.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        std::cerr << "Failed to bind loginHashFrom for delete blobs: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(deleteBlobsStmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (sqlite3_step(deleteBlobsStmt) != SQLITE_DONE) {
        std::cerr << "Failed to delete blobs: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(deleteBlobsStmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    sqlite3_finalize(deleteBlobsStmt);

    
    const char* insertBlobSql = "INSERT INTO BLOBS(BLOB_UID, LOGIN_HASH_TO, LOGIN_HASH_FROM, "
        "FILES_COUNT_IN_BLOB, FILES_RECEIVED, FILES_SENT) "
        "VALUES(?, ?, ?, ?, ?, ?);";

    const char* insertFileSql = "INSERT INTO BLOB_FILES(BLOB_UID, FILE_PACKET, FILE_ID) VALUES(?, ?, ?);";

    sqlite3_stmt* blobStmt = nullptr;
    sqlite3_stmt* fileStmt = nullptr;

    if (sqlite3_prepare_v2(m_db, insertBlobSql, -1, &blobStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare blob insert statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    if (sqlite3_prepare_v2(m_db, insertFileSql, -1, &fileStmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare file insert statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(blobStmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& blob : newBlobs) {
        sqlite3_bind_text(blobStmt, 1, blob.blobUID.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(blobStmt, 2, blob.receiverLoginHash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(blobStmt, 3, blob.senderLoginHash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(blobStmt, 4, blob.filesCountInBlob);
        sqlite3_bind_int(blobStmt, 5, blob.filesReceived);
        sqlite3_bind_int(blobStmt, 6, blob.filesSent);

        if (sqlite3_step(blobStmt) != SQLITE_DONE) {
            std::cerr << "Failed to insert blob: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(blobStmt);
            sqlite3_finalize(fileStmt);
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        sqlite3_reset(blobStmt);

        for (const auto& filePacket : blob.filePacketsVec) {
            std::string fileId;

            size_t firstNewLine = filePacket.find('\n');
            size_t secondNewLine = filePacket.find('\n', firstNewLine + 1);
            fileId = filePacket.substr(firstNewLine + 1, secondNewLine - (firstNewLine + 1));

            sqlite3_bind_text(fileStmt, 1, blob.blobUID.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(fileStmt, 2, filePacket.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(fileStmt, 3, fileId.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(fileStmt) != SQLITE_DONE) {
                std::cerr << "Failed to insert blob file: " << sqlite3_errmsg(m_db) << std::endl;
                sqlite3_finalize(blobStmt);
                sqlite3_finalize(fileStmt);
                sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
                return false;
            }
            sqlite3_reset(fileStmt);
        }
    }

    sqlite3_finalize(blobStmt);
    sqlite3_finalize(fileStmt);

    if (sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
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

User* Database::getUser(CryptoPP::SecByteBlock avatarsKey, CryptoPP::RSA::PrivateKey privateKey, const std::string& loginHash) {
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

            user = new User(login, dbLoginHash, passwordHash, name, isHasPhoto, new Avatar(avatarsKey, photoPath));
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

std::vector<User*> Database::getUsers(CryptoPP::SecByteBlock avatarsKey, CryptoPP::RSA::PrivateKey privateKey) {
    std::vector<User*> users;

    if (!m_db) {
        std::cerr << "Database not initialized" << std::endl;
        return users;
    }

    const std::string sql = "SELECT "
        "LOGIN_HASH, LOGIN, NAME, PASSWORD_HASH, "
        "ENCRYPTION_PART, LAST_SEEN, PUBLIC_KEY, "
        "IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE "
        "FROM USER";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return users;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
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
            std::string photoSize = encryptedPhotoSize.empty() ? "" : crypto::RSADecrypt(privateKey, encryptedPhotoSize);

            User* user = new User(login, dbLoginHash, passwordHash, name, isHasPhoto, new Avatar(avatarsKey, photoPath));
            user->setEncryptionPart(encryptionPart);
            user->setLastSeen(lastSeen);

            if (!publicKeyStr.empty()) {
                user->setPublicKey(crypto::deserializePublicKey(publicKeyStr));
            }

            users.push_back(user);
        }
        catch (const std::exception& e) {
            std::cerr << "Error processing user data: " << e.what() << std::endl;
        }
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "SQL error while iterating: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return users;
}


std::string Database::safeColumnText(sqlite3_stmt* stmt, int column) {
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, column));
    return text ? text : "";
}

std::vector<std::string> Database::getUsersStatusesVec(CryptoPP::SecByteBlock avatarsKey, CryptoPP::RSA::PrivateKey privateKey, const std::vector<std::string>& loginsVec, const std::map<std::string, User*>& mapOnlineUsers) {
    std::vector<std::string> statuses;
    for (const auto& login : loginsVec) {

        auto it = mapOnlineUsers.find(login);
        if (it != mapOnlineUsers.end()) {
            statuses.push_back("online"); 
            continue;
        }

        std::string statusFromDb = getUser(avatarsKey, privateKey, login)->getLastSeen();
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


void Database::collect(const std::string& loginHashTo, const std::string& loginHashFrom,
    const std::string& packet, QueryType type) {
    const char* sql = "INSERT INTO COLLECTED_PACKETS (LOGIN_HASH_TO, LOGIN_HASH_FROM, PACKET, PACKET_TYPE) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* stmt = nullptr;
    int rc;

    rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, loginHashTo.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, loginHashFrom.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, packet.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, static_cast<int>(type));

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
    const char* selectSql = "SELECT PACKET, PACKET_TYPE FROM COLLECTED_PACKETS "
        "WHERE LOGIN_HASH_TO = ? "
        "ORDER BY id ASC;";  

    const char* deleteSql = "DELETE FROM COLLECTED_PACKETS WHERE LOGIN_HASH_TO = ? AND PACKET = ?;";

    sqlite3_stmt* selectStmt = nullptr;
    sqlite3_stmt* deleteStmt = nullptr;
    int rc;
    std::vector<std::pair<std::string, QueryType>> packets;

    rc = sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(m_db) << std::endl;
        return packets;
    }

    rc = sqlite3_prepare_v2(m_db, selectSql, -1, &selectStmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare SELECT statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return packets;
    }

    rc = sqlite3_bind_text(selectStmt, 1, loginHash.c_str(), -1, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to bind login in SELECT statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(selectStmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return packets;
    }

    while ((rc = sqlite3_step(selectStmt)) == SQLITE_ROW) {
        const char* packet = reinterpret_cast<const char*>(sqlite3_column_text(selectStmt, 0));
        if (!packet) continue;

        QueryType type = static_cast<QueryType>(sqlite3_column_int(selectStmt, 1));
        packets.emplace_back(packet, type);
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "SELECT execution failed: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_finalize(selectStmt);
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return packets;
    }

    sqlite3_finalize(selectStmt);

    for (const auto& [packet, type] : packets) {
        rc = sqlite3_prepare_v2(m_db, deleteSql, -1, &deleteStmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "Failed to prepare DELETE statement: " << sqlite3_errmsg(m_db) << std::endl;
            continue;
        }

        rc = sqlite3_bind_text(deleteStmt, 1, loginHash.c_str(), -1, SQLITE_STATIC);
        rc |= sqlite3_bind_text(deleteStmt, 2, packet.c_str(), -1, SQLITE_STATIC);

        if (rc != SQLITE_OK) {
            std::cerr << "Failed to bind parameters in DELETE statement: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(deleteStmt);
            continue;
        }

        rc = sqlite3_step(deleteStmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to delete packet: " << sqlite3_errmsg(m_db) << std::endl;
        }

        sqlite3_finalize(deleteStmt);
    }

    rc = sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
    }

    return packets;
}

std::vector<PacketData> Database::getPacketsBySender(const std::string& loginHashFrom) {
    std::vector<PacketData> packets;
    const char* sql = "SELECT LOGIN_HASH_TO, LOGIN_HASH_FROM, PACKET, PACKET_TYPE "
        "FROM COLLECTED_PACKETS "
        "WHERE LOGIN_HASH_FROM = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return packets;
    }

    sqlite3_bind_text(stmt, 1, loginHashFrom.c_str(), -1, SQLITE_STATIC);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        PacketData data;
        data.loginHashTo = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        data.loginHashFrom = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        data.packet = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        data.type = static_cast<QueryType>(sqlite3_column_int(stmt, 3));

        packets.push_back(data);
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Error during retrieval: " << sqlite3_errmsg(m_db) << std::endl;
    }

    sqlite3_finalize(stmt);
    return packets;
}

bool Database::replaceAllPackets(const std::string& loginHashFrom, const std::vector<PacketData>& newPackets) {
    const char* beginTransaction = "BEGIN TRANSACTION;";
    int rc = sqlite3_exec(m_db, beginTransaction, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    const char* deleteSql = "DELETE FROM COLLECTED_PACKETS WHERE LOGIN_HASH_FROM = ?;";
    sqlite3_stmt* deleteStmt = nullptr;

    rc = sqlite3_prepare_v2(m_db, deleteSql, -1, &deleteStmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare delete statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    sqlite3_bind_text(deleteStmt, 1, loginHashFrom.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(deleteStmt);
    sqlite3_finalize(deleteStmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to delete packets: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    const char* insertSql = "INSERT INTO COLLECTED_PACKETS "
        "(LOGIN_HASH_TO, LOGIN_HASH_FROM, PACKET, PACKET_TYPE) "
        "VALUES (?, ?, ?, ?);";

    sqlite3_stmt* insertStmt = nullptr;
    rc = sqlite3_prepare_v2(m_db, insertSql, -1, &insertStmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare insert statement: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    for (const auto& packet : newPackets) {
        sqlite3_bind_text(insertStmt, 1, packet.loginHashTo.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(insertStmt, 2, packet.loginHashFrom.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(insertStmt, 3, packet.packet.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(insertStmt, 4, static_cast<int>(packet.type));

        rc = sqlite3_step(insertStmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Failed to insert packet: " << sqlite3_errmsg(m_db) << std::endl;
            sqlite3_finalize(insertStmt);
            sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }

        sqlite3_reset(insertStmt);
    }

    sqlite3_finalize(insertStmt);

    const char* commitTransaction = "COMMIT;";
    rc = sqlite3_exec(m_db, commitTransaction, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(m_db) << std::endl;
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }

    return true;
}



std::vector<User*> Database::findUsers(CryptoPP::SecByteBlock avatarsKey, const CryptoPP::RSA::PrivateKey& privateKey,
    const std::string& currentUserLoginHash,
    const std::string& searchText,
    std::vector<User*>& foundUsers) {

    const char* sql =
        "SELECT LOGIN_HASH, LOGIN, NAME, PASSWORD_HASH, ENCRYPTION_PART, "
        "LAST_SEEN, PUBLIC_KEY, IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE FROM USER "
        "WHERE LOGIN_HASH != ?;";

    sqlite3_stmt* stmt = nullptr;

    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(m_db) << std::endl;
        return foundUsers;
    }

    sqlite3_bind_text(stmt, 1, currentUserLoginHash.c_str(), -1, SQLITE_TRANSIENT);

    std::string searchTextLower = searchText;
    std::transform(searchTextLower.begin(), searchTextLower.end(), searchTextLower.begin(), ::tolower);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* encLogin = sqlite3_column_text(stmt, 1);
        const unsigned char* encName = sqlite3_column_text(stmt, 2);

        std::string decryptedLogin = crypto::RSADecrypt(privateKey, reinterpret_cast<const char*>(encLogin));
        std::string decryptedName = crypto::RSADecrypt(privateKey, reinterpret_cast<const char*>(encName));

        std::string decryptedLoginLower = decryptedLogin;
        std::transform(decryptedLoginLower.begin(), decryptedLoginLower.end(), decryptedLoginLower.begin(), ::tolower);
        std::string decryptedNameLower = decryptedName;
        std::transform(decryptedNameLower.begin(), decryptedNameLower.end(), decryptedNameLower.begin(), ::tolower);

        if (decryptedLoginLower.find(searchTextLower) == std::string::npos &&
            decryptedNameLower.find(searchTextLower) == std::string::npos) {
            continue;
        }

        User* user = new User();

        user->setLoginHash(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        user->setLogin(decryptedLogin);
        user->setName(decryptedName);
        user->setPasswordHash(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)));
        user->setEncryptionPart(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)));
        user->setLastSeen(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        user->setPublicKey(crypto::deserializePublicKey(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6))));
        user->setIsHasAvatar(sqlite3_column_int(stmt, 7) != 0);

        std::string photoPath = crypto::RSADecrypt(privateKey, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)));
        Avatar* avatar = new Avatar(avatarsKey, photoPath);
        user->setAvatar(avatar);
        if (avatar->getPath() != "") {
            user->setIsHasAvatar(true);
        }

        foundUsers.push_back(user);

        if (foundUsers.size() >= 20) {
            break;
        }
    }

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
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

void Database::updateUserPublicKey(const std::string& loginHash, const std::string& publicKey) {
    const char* sql = "UPDATE USER SET PUBLIC_KEY = ? WHERE LOGIN_HASH = ?";
    executeUpdate(sql, { publicKey, loginHash });
}

void Database::updateUserAvatar(CryptoPP::RSA::PublicKey publicKey, const std::string& loginHash, const std::string& avatarPath, size_t photoSize) {
    const char* sql = "UPDATE USER SET IS_HAS_PHOTO = ?, PHOTO_PATH = ?, PHOTO_SIZE = ? WHERE LOGIN_HASH = ?";

    std::vector<std::string> params;
    params.push_back("1"); 
    params.push_back(crypto::RSAEncrypt(publicKey, avatarPath));
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

void Database::executeSQL(const char* sql, const char* tableName) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error in " << tableName << ": " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
    else {
        std::cout << "Table " << tableName << " created/updated successfully" << std::endl;
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
