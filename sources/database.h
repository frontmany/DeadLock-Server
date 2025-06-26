#pragma once

#include <iostream>
#include <ctime>
#include <iomanip>  
#include <sstream>
#include <sstream>
#include <thread>
#include <map>
#include <string>
#include <vector>
#include <mutex>
#include <random>

#include "asio.hpp"
#include "photo.h"  
#include "sqlite/sqlite3.h" 

class User;
class Photo;
enum class QueryType : uint32_t;

class Database {
public:
	Database() = default;
	void init();

	User* getUser(const std::string& loginHash);
	void addUser(const std::string& loginHash, const std::string& passwordHash, const std::string& encryptionPart, const std::string& lastSeen);

    void updateUserLogin(const std::string& loginHash, const std::string& login);
    void updateUserName(const std::string& loginHash, const std::string& name);
    void updateUserPassword(const std::string& loginHash, const std::string& passwordHash);
    void updateUserEncryptionPart(const std::string& loginHash, const std::string& encryptionPart);
    void updateUserLastSeen(const std::string& loginHash, const std::string& lastSeen);
    void updateUserPublicKey(const std::string& loginHash, const std::string& publicKey);
	void updateUserPhoto(const std::string& loginHash, const Photo& photo, size_t photoSize);




	void collect(const std::string& loginHash, const std::string& packet, QueryType type);
	std::vector<std::pair<std::string, QueryType>> getCollected(const std::string& loginHash);
	std::vector<std::string> getUsersStatusesVec(const std::vector<std::string>& loginsVec, const std::map<std::string, User*>& mapOnlineUsers);
	std::vector<User*> findUsers(const std::string& currentUserLoginHash, const std::string& searchText, std::vector<User*>& foundUsers);


	bool checkPassword(const std::string& loginHash, const std::string& passwordHash);
	bool checkNewLogin(const std::string& newLoginHash);
	std::string getCurrentDateTime();

private:
	void executeUpdate(const char* sql, const std::vector<std::string>& params);
	void executeAndCheck(sqlite3_stmt* stmt, const std::string& operation);
	std::string friendsToString(const std::vector<std::string>& friends);
	std::vector<std::string> stringToFriends(const std::string& friendsString);

private:
	sqlite3* m_db;
};