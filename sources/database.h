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
#include "blob.h"  
#include "rsa.h"  
#include "sqlite/sqlite3.h" 

class User;
class Photo;
enum class QueryType : uint32_t;

class Database {
public:
	Database() = default;
	void init();

	bool addBlob(const std::string& blobUid, const std::string& loginHash, int filesCountInBlob);
	bool removeBlob(const std::string& blobUid);
	bool isBlobExists(const std::string& blobUid);
	bool addFileToBlob(const std::string& blobUid, const std::string& fileId, const std::string& filePacket);
	bool incrementFilesReceivedCounter(const std::string& blobUid);
	bool incrementFilesSentCounter(const std::string& blobUid);
	Blob getBlob(const std::string& blobUid);
	std::vector<Blob> getBlobs(const std::string& loginHash);


	User* getUser(CryptoPP::RSA::PrivateKey privateKey, const std::string& loginHash);
	bool addUser(const std::string& loginHash, const std::string& passwordHash, const std::string& encryptionPartEnc, const std::string& lastSeenEnc);
    bool updateUserLogin(const CryptoPP::RSA::PublicKey& publicKey, const std::string& loginHash, const std::string& newLogin);
    void updateUserName(const std::string& loginHash, const std::string& nameEnc);
    void updateUserPassword(const std::string& loginHash, const std::string& passwordHash);
    void updateUserEncryptionPart(const std::string& loginHash, const std::string& encryptionPartEnc);
    void updateUserLastSeen(const std::string& loginHash, const std::string& lastSeenEnc);
    void updateUserPublicKey(const std::string& loginHash, const std::string& publicKey);
	void updateUserPhoto(CryptoPP::RSA::PublicKey privateKey, const std::string& loginHash, const Photo& photo, size_t photoSize);
	void updateUserLoginOnly(const std::string& loginHash, const std::string& newLoginEnc);
	std::vector<std::string> getUsersStatusesVec(CryptoPP::RSA::PrivateKey privateKey, const std::vector<std::string>& loginsVec, const std::map<std::string, User*>& mapOnlineUsers);
	std::vector<User*> findUsers(const CryptoPP::RSA::PrivateKey& privateKey, const std::string& currentUserLoginHash, const std::string& searchText, std::vector<User*>& foundUsers);


	void collect(const std::string& loginHash, const std::string& packet, QueryType type);
	std::vector<std::pair<std::string, QueryType>> getCollected(const std::string& loginHash);
	

	bool checkPassword(const std::string& loginHash, const std::string& passwordHash);
	bool checkNewLogin(const std::string& newLoginHash);
	std::string getCurrentDateTime();


private:
	void executeSQL(const char* sql, const char* tableName);
	std::string safeColumnText(sqlite3_stmt* stmt, int column);
	void executeUpdate(const char* sql, const std::vector<std::string>& params);
	std::string friendsToString(const std::vector<std::string>& friends);
	std::vector<std::string> stringToFriends(const std::string& friendsString);

private:
	sqlite3* m_db;
};