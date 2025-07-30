#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sqlite3.h>
#include <rsa.h> 
#include "crypto.h"

std::string readKeyFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open key file: " + filename);
    }
    std::string key((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();
    return key;
}

void printUserTable(sqlite3* db, const CryptoPP::RSA::PrivateKey& privateKey) {
    std::cout << "\nTable USER:\n";
    std::cout << "----------------------------------------------------------------------------------------------------\n";

    const char* sql = "SELECT LOGIN_HASH, LOGIN, NAME, PASSWORD_HASH, ENCRYPTION_PART, "
        "LAST_SEEN, PUBLIC_KEY, IS_HAS_PHOTO, PHOTO_PATH, PHOTO_SIZE FROM USER;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << "| LOGIN_HASH | LOGIN | NAME | PASSWORD_HASH | ENCRYPTION_PART | LAST_SEEN | PUBLIC_KEY | HAS_PHOTO | PHOTO_PATH | PHOTO_SIZE |\n";
    std::cout << "----------------------------------------------------------------------------------------------------\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string loginHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));

        std::string login = crypto::RSADecrypt(privateKey, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
        std::string name = crypto::RSADecrypt(privateKey, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));
        std::string passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        std::string encryptionPart = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        std::string lastSeen = crypto::RSADecrypt(privateKey, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)));
        std::string publicKey = sqlite3_column_text(stmt, 6) ? reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)) : "";
        bool hasPhoto = sqlite3_column_int(stmt, 7) != 0;
        std::string photoPath = crypto::RSADecrypt(privateKey, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8)));
        std::string photoSize = crypto::RSADecrypt(privateKey, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9)));

        std::cout << "| " << loginHash << " | " << login << " | " << name << " | "
            << passwordHash << " | " << encryptionPart << " | " << lastSeen << " | "
            << (publicKey.empty() ? "NULL" : "PUBLIC_KEY") << " | " << hasPhoto << " | "
            << photoPath << " | " << photoSize << " |\n\n\n";
    }

    sqlite3_finalize(stmt);
    std::cout << "----------------------------------------------------------------------------------------------------\n";
}

void printPacketsTable(sqlite3* db) {
    std::cout << "\nTable COLLECTED_PACKETS:\n";
    std::cout << "--------------------------------------------------------\n";

    const char* sql = "SELECT id, LOGIN_HASH_TO, LOGIN_HASH_FROM, PACKET, PACKET_TYPE FROM COLLECTED_PACKETS;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << "| ID | TO | FROM | PACKET (truncated) | TYPE |\n";
    std::cout << "--------------------------------------------------------\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        std::string to = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string from = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        std::string packet = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        int type = sqlite3_column_int(stmt, 4);

        std::string packetShort = packet.substr(0, 20) + (packet.length() > 20 ? "..." : "");

        std::cout << "| " << id << " | " << to << " | " << from << " | "
            << packetShort << " | " << type << " |\n";
    }

    sqlite3_finalize(stmt);
    std::cout << "--------------------------------------------------------\n";
}

void printBlobsTable(sqlite3* db) {
    std::cout << "\nTable BLOBS:\n";
    std::cout << "--------------------------------------------------------------------\n";

    const char* sql = "SELECT BLOB_UID, LOGIN_HASH_TO, LOGIN_HASH_FROM, "
        "FILES_COUNT_IN_BLOB, FILES_RECEIVED, FILES_SENT FROM BLOBS;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << "| BLOB_UID | TO | FROM | COUNT | RECEIVED | SENT |\n";
    std::cout << "--------------------------------------------------------------------\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string uid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string to = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string from = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int count = sqlite3_column_int(stmt, 3);
        int received = sqlite3_column_int(stmt, 4);
        int sent = sqlite3_column_int(stmt, 5);

        std::cout << "| " << uid << " | " << to << " | " << from << " | "
            << count << " | " << received << " | " << sent << " |\n";
    }

    sqlite3_finalize(stmt);
    std::cout << "--------------------------------------------------------------------\n";
}

void printBlobFilesTable(sqlite3* db) {
    std::cout << "\nTable BLOB_FILES:\n";
    std::cout << "-----------------------------------------------\n";

    const char* sql = "SELECT BLOB_UID, FILE_ID, FILE_PACKET FROM BLOB_FILES;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        return;
    }

    std::cout << "| BLOB_UID | FILE_ID | FILE_PACKET (truncated) |\n";
    std::cout << "-----------------------------------------------\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string uid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string fileId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string packet = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

        std::string packetShort = packet.substr(0, 20) + (packet.length() > 20 ? "..." : "");

        std::cout << "| " << uid << " | " << fileId << " | " << packetShort << " |\n";
    }

    sqlite3_finalize(stmt);
    std::cout << "-----------------------------------------------\n";
}

void deleteUserAndPackets(sqlite3* db, const std::string& login) {
    // Рассчитываем хэш логина для поиска в базе
    std::string loginHash = crypto::calculateHash(login);

    // Начинаем транзакцию
    char* errMsg = nullptr;
    if (sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << (errMsg ? errMsg : "") << std::endl;
        if (errMsg) sqlite3_free(errMsg);
        return;
    }

    try {
        // Удаляем все пакеты для этого пользователя (как отправленные, так и полученные)
        const char* deletePacketsSql = "DELETE FROM COLLECTED_PACKETS WHERE LOGIN_HASH_TO = ? OR LOGIN_HASH_FROM = ?;";
        sqlite3_stmt* deletePacketsStmt = nullptr;

        if (sqlite3_prepare_v2(db, deletePacketsSql, -1, &deletePacketsStmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare delete packets statement: " + std::string(sqlite3_errmsg(db)));
        }

        sqlite3_bind_text(deletePacketsStmt, 1, loginHash.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(deletePacketsStmt, 2, loginHash.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(deletePacketsStmt) != SQLITE_DONE) {
            throw std::runtime_error("Failed to delete packets: " + std::string(sqlite3_errmsg(db)));
        }
        sqlite3_finalize(deletePacketsStmt);

        // Удаляем пользователя
        const char* deleteUserSql = "DELETE FROM USER WHERE LOGIN_HASH = ?;";
        sqlite3_stmt* deleteUserStmt = nullptr;

        if (sqlite3_prepare_v2(db, deleteUserSql, -1, &deleteUserStmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare delete user statement: " + std::string(sqlite3_errmsg(db)));
        }

        sqlite3_bind_text(deleteUserStmt, 1, loginHash.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(deleteUserStmt) != SQLITE_DONE) {
            throw std::runtime_error("Failed to delete user: " + std::string(sqlite3_errmsg(db)));
        }

        int changes = sqlite3_changes(db);
        sqlite3_finalize(deleteUserStmt);

        if (changes == 0) {
            std::cout << "User with login '" << login << "' not found." << std::endl;
        }
        else {
            std::cout << "Successfully deleted user '" << login << "' and all related packets." << std::endl;
        }

        // Коммитим транзакцию
        if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
            throw std::runtime_error("Failed to commit transaction: " + std::string(errMsg ? errMsg : ""));
        }
        if (errMsg) sqlite3_free(errMsg);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        if (errMsg) sqlite3_free(errMsg);
    }
}

int main() {
    sqlite3* db;
    int rc = sqlite3_open("Database.db", &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    try {
        std::string privateKeyStr = readKeyFromFile("private_key.txt");
        CryptoPP::RSA::PrivateKey privateKey = crypto::deserializePrivateKey(privateKeyStr);

        printUserTable(db, privateKey);
        printPacketsTable(db);
        printBlobsTable(db);
        printBlobFilesTable(db);

        // Цикл для удаления пользователей
        while (true) {
            std::cout << "\nEnter login to delete (or 'exit' to quit): ";
            std::string input;
            std::getline(std::cin, input);

            if (input == "exit" || input == "quit") {
                break;
            }

            // Подтверждение удаления
            std::cout << "Are you sure you want to delete user '" << input << "' and all their packets? (y/n): ";
            std::string confirm;
            std::getline(std::cin, confirm);

            if (confirm == "y" || confirm == "Y") {
                deleteUserAndPackets(db, input);

                // Перепечатываем таблицы после удаления
                printUserTable(db, privateKey);
                printPacketsTable(db);
            }
            else {
                std::cout << "Deletion canceled." << std::endl;
            }
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}