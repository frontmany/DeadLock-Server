#include "photo.h"
#include "base64.h"



std::string Photo::wideStringToString(const WCHAR* wideStr) {
    if (!wideStr) {
        return ""; // Возвращаем пустую строку, если входной указатель нулевой
    }

    // Получаем размер буфера, необходимого для преобразования
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) {
        return ""; // Возвращаем пустую строку в случае ошибки
    }

    // Создаем строку с нужным размером
    std::string str(size_needed - 1, 0); // Исключаем завершающий нулевой символ

    // Выполняем преобразование
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

 
std::string Photo::serialize() const {
    if (m_photoPath.empty()) {
        return "";
    }

    std::ifstream file(m_photoPath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + m_photoPath);
    }

    std::vector<char> buffer(m_size);
    file.read(buffer.data(), m_size);
    file.close();

    // Кодируем бинарные данные в Base64
    return base64_encode(std::string(buffer.data(), m_size), false);
}

Photo Photo::deserialize(const std::string& data, size_t size, std::string login) {
    if (data.empty()) {
        return Photo();
    }

    std::istringstream iss(data);

    // Чтение данных фото
    std::vector<char> buffer(size);
    iss.read(buffer.data(), size);
    if (!iss) {
        std::cerr << "Failed to read photo data" << std::endl;
        return Photo();
    }

    WCHAR username[256];
    DWORD username_len = sizeof(username) / sizeof(WCHAR);
    if (!GetUserNameW(username, &username_len)) {
        std::cerr << "Failed to get user name" << std::endl;
        return Photo();
    }


    std::string usernameStr = wideStringToString(username);
    std::string saveDirectory = "C:/Users/" + usernameStr + "/Documents/ReceivedFiles";
    std::string tempPath = saveDirectory + "/" + login + "Photo.png";

    if (size > 0) {
        if (!std::filesystem::create_directories(saveDirectory)) {
            std::cerr << "Failed to create directories: " << saveDirectory << std::endl;
        }

        std::ofstream outFile(tempPath, std::ios::binary);
        if (!outFile) {
            std::cerr << "Failed to open file for writing: " << tempPath << std::endl;
            return Photo();
        }

        outFile.write(buffer.data(), size);
        if (!outFile) {
            std::cerr << "Failed to write data to file: " << tempPath << std::endl;
            return Photo();
        }

        outFile.close();
        std::cout << "Photo saved to: " << tempPath << std::endl;
    }
    else {
        std::cout << "No photo data to restore" << std::endl;
        return Photo("");
    }
    Photo ph = Photo(tempPath);
    ph.updateSize();
    return ph;
}