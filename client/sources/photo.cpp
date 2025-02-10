#include "photo.h"

Photo::Photo(const std::string& photoPath)
    : m_photoPath(photoPath), m_size(-1) {
    if (photoPath != "") {
        updateSize();
        saveToFile();
    }
}

std::string Photo::wideStringToString(const WCHAR* wideStr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, &str[0], size_needed, nullptr, nullptr);
    return str;
}


void Photo::updateSize() {
    std::ifstream file(m_photoPath, std::ios::binary);
    if (file) {
        file.seekg(0, std::ios::end);
        m_size = static_cast<std::size_t>(file.tellg());
        file.close();
    }
    else {
        m_size = 0;
    }
}


std::string Photo::serialize() {
    if (m_photoPath != "") {
        std::ostringstream oss;
        oss.write(reinterpret_cast<const char*>(&m_size), sizeof(m_size));
        std::ifstream file(m_photoPath, std::ios::binary);
        if (file) {
            std::vector<char> buffer(m_size);
            file.read(buffer.data(), m_size);
            oss.write(buffer.data(), m_size);
            file.close();
        }
        return oss.str();
    }
    else {
        return "no photo";
    }

}


Photo Photo::deserialize(const std::string& data) {
    if (data == "no photo") {
        return Photo();
    }

    std::istringstream iss(data);
    std::size_t size;

    iss.read(reinterpret_cast<char*>(&size), sizeof(size));
    WCHAR username[256];
    DWORD username_len = sizeof(username) / sizeof(WCHAR);
    if (!GetUserNameW(username, &username_len)) {
        std::cout << "No User data" << std::endl;
    }

    std::string usernameStr = wideStringToString(username);
    std::string saveDirectory = "C:\\Users\\" + usernameStr + "\\Documents\\Data_Air_Gram\\Photos";
    std::string tempPath = saveDirectory + "\\restored_photo.jpg";


    std::vector<char> buffer(size);

    iss.read(buffer.data(), size);
    std::filesystem::create_directories(saveDirectory);
    std::ofstream outFile(tempPath, std::ios::binary);
    if (outFile) {
        outFile.write(buffer.data(), size);
        outFile.close();
        std::cout << tempPath << std::endl;
    }
    else {
        std::cerr << "Open file Error" << std::endl;
    }


    return Photo(tempPath);
}


void Photo::saveToFile() const {
    WCHAR username[256];
    DWORD username_len = sizeof(username) / sizeof(WCHAR);
    if (GetUserNameW(username, &username_len)) {
        std::string usernameStr = wideStringToString(username);
        std::string saveDirectory = "C:\\Users\\" + usernameStr + "\\Documents\\Data_Air_Gram\\Photos";
        std::filesystem::path filePath(m_photoPath);
        std::string fileName = filePath.filename().string();
        std::filesystem::path fullPath = std::filesystem::path(saveDirectory) / fileName;
        std::filesystem::path dir = fullPath.parent_path();

        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::ifstream src(m_photoPath, std::ios::binary);
        std::ofstream dst(fullPath.string(), std::ios::binary);
        if (src && dst) {
            dst << src.rdbuf();
        }
        else {
            std::cerr << "Open file Error" << std::endl;
        }
    }
    else {
        std::cerr << "No User data" << std::endl;
    }
}

