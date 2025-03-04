#include<string>
#include<functional>

namespace hash {

	std::string hashPassword(std::string password);
	bool verifyPassword(std::string password, std::string storedHash);

};