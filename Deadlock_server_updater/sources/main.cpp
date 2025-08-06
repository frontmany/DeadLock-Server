#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <regex>

static const char* versionsListPath = "./versions/versionsList.txt";
static const char* folderName = "versions";

void addNewVersion(const std::string& inputVersion = "") {
    std::ifstream infile(versionsListPath);
    if (!infile) {
        std::cerr << "Cannot open " << versionsListPath << " for reading\n";
        return;
    }

    std::string lastLine;
    std::string line;
    while (std::getline(infile, line)) {
        if (!line.empty())
            lastLine = line;
    }
    infile.close();

    std::string prefix;
    int major = 0, minor = 0, patch = 0;

    std::istringstream iss(lastLine);
    iss >> prefix;
    std::string versionStr;
    iss >> versionStr;

    if (versionStr.empty()) {
        std::cerr << "Incorrect format of the latest version\n";
        return;
    }

    char dot1, dot2;
    std::istringstream vss(versionStr);
    if (!(vss >> major >> dot1 >> minor >> dot2 >> patch) || dot1 != '.' || dot2 != '.') {
        std::cerr << "Version parsing error\n";
        return;
    }

    std::string input;
    std::string newVersion;

    if (!inputVersion.empty()) {
        input = inputVersion;
    }
    else {
        std::cout << "Enter the update type or the full version: ";
        std::cin >> input;
    }

    if (input == "hotfix") {
        patch += 1;
        newVersion = prefix + " " + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
    else if (input == "minor") {
        minor += 1;
        patch = 0;
        newVersion = prefix + " " + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
    else if (input == "major") {
        major += 1;
        minor = 0;
        patch = 0;
        newVersion = prefix + " " + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
    else {
        std::regex versionRegex(R"(^(\d+)\.(\d+)\.(\d+)$)");
        std::smatch match;
        if (std::regex_match(input, match, versionRegex)) {
            newVersion = prefix + " " + input;
        }
        else {
            std::cerr << "Incorrect input. Enter hotfix, minor, major, or the X.Y.Z version.\n";
            return;
        }
    }

    std::ofstream outfile(versionsListPath, std::ios::app);
    if (!outfile) {
        std::cerr << "Error opening the file for writing\n";
        return;
    }
    outfile << newVersion << "\n";
    std::cout << "A new version has been added: " << newVersion << "\n";
}

int main(int argc, char* argv[]) {
    try {
        if (std::filesystem::create_directory(folderName)) {
            std::cout << "versions folder created\n";
        }
        if (!std::filesystem::exists(versionsListPath)) {
            std::ofstream file(versionsListPath);
            if (!file) {
                std::cerr << "Error when creating the versionsList.txt\n";
                return 1;
            }

            file << "Deadlock 1.1.0\n";
            file.close();
        }

        std::cout << "----------------------------------------\n";

        if (argc > 1) {
            addNewVersion(argv[1]);
        }
        else {
            addNewVersion();
        }

        std::cout << "----------------------------------------\n";
        std::cout << "New version has been added\n";

    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "error: " << e.what() << '\n';
    }
    return 0;
}