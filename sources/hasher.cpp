#include"hasher.h"

std::string hash::hashPassword(std::string password) {
	std::hash<std::string> hashFunction;
	return std::to_string(hashFunction(password));
}

bool hash::verifyPassword(std::string passwordHash, std::string storedHash) {
	if (passwordHash == storedHash) {
		return true;
	}
	else if (passwordHash != storedHash){
		return false;
	}
}
