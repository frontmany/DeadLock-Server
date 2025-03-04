#include"hasher.h"

std::string hash::hashPassword(std::string password) {
	std::hash<std::string> hashFunction;
	return std::to_string(hashFunction(password));
}

bool hash::verifyPassword(std::string password, std::string storedHash) {
	std::hash<std::string> hashFunction;
	std::string hashToCheck = std::to_string(hashFunction(password));
	if (hashToCheck == storedHash) {
		return true;
	}
	else if (hashToCheck != storedHash){
		return false;
	}
}
