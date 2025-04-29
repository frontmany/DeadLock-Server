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

	User* getUser(const std::string& login);
	void addUser(const std::string& login, const std::string& lastSeen, const std::string& name, const std::string& passwordHash);

	void collect(const std::string& login, const std::string& packet, QueryType type);
	std::vector<std::pair<std::string, QueryType>> getCollected(const std::string& login);
	std::vector<std::string> getUsersStatusesVec(const std::vector<std::string>& loginsVec, const std::map<std::string, User*>& mapOnlineUsers);

	void updateUserName(const std::string& login, const std::string& newName);
	void updateUserPassword(const std::string& login, const std::string& passwordHash);
	void updateUserPhoto(const std::string& login, const Photo& photo, size_t photoSize);
	void updateUserLogin(const std::string& oldLogin, const std::string& newLogin);
	void updateUserStatus(const std::string& login, std::string lastSeen);

	bool checkPassword(const std::string& login, const std::string& passwordHash);
	bool checkNewLogin(const std::string& newLogin);
	std::string getCurrentDateTime();

private:
	void executeAndCheck(sqlite3_stmt* stmt, const std::string& operation);
	std::string friendsToString(const std::vector<std::string>& friends);
	std::vector<std::string> stringToFriends(const std::string& friendsString);

private:
	sqlite3* m_db;
};