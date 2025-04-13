#include<string>
#include<functional>

namespace hash {

	std::string hashPassword(std::string password);
	bool verifyPassword(std::string passwordHash, std::string storedHash);

};