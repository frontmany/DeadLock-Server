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

class Database {
public:
	Database() = default;
	void init();
	User* getUser(const std::string& login, asio::ip::tcp::socket* acceptSocket = nullptr);
	void addUser(const std::string& login, const std::string& lastSeen, const std::string& name, const std::string& passwordHash);
	void collect(const std::string& login, const std::string& packet);

	void updateUserName(const std::string& login, const std::string& newName);
	void updateUserPassword(const std::string& login, const std::string& passwordHash);
	void updateUserPhoto(const std::string& login, const Photo& photo, size_t photoSize);

	void updateUserStatus(const std::string& login, std::string lastSeen);
	std::vector<std::string> getCollected(const std::string& login);
	bool checkPassword(const std::string& login, const std::string& passwordHash);
	std::vector<std::string> getUsersStatusesVec(const std::vector<std::string>& loginsVec, const std::map<std::string, User*>& mapOnlineUsers);
	std::string getCurrentDateTime();
	bool checkIsNewLoginAvailable(const std::string& newLogin);
	void executeAndCheck(sqlite3_stmt* stmt, const std::string& operation);
private:
	std::string friendsToString(const std::vector<std::string>& friends);
	std::vector<std::string> stringToFriends(const std::string& friendsString);

private:
	sqlite3* m_db;
};