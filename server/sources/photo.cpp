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

Photo Photo::deserialize(const std::string& data, size_t size, std::string oldLogin, std::string newLogin) {
    // Если данных о фото нет - возвращаем пустое фото
    if (data.empty()) {
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
    std::filesystem::create_directories(saveDirectory);

    std::string newPath = saveDirectory + "/" + newLogin + "Photo.png";
    std::string oldPath = saveDirectory + "/" + oldLogin + "Photo.png";

    if (newLogin != oldLogin) {
        // Удаляем старое фото, если оно существует
        if (std::filesystem::exists(oldPath)) {
            try {
                std::filesystem::remove(oldPath);
                std::cout << "Old login photo deleted: " << oldPath << std::endl;
            }
            catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Failed to delete old login photo: " << e.what() << std::endl;
            }
        }
    }
    

    // Удаляем существующее фото с новым логином (если есть)
    if (std::filesystem::exists(newPath)) {
        try {
            std::filesystem::remove(newPath);
            std::cout << "Existing photo deleted for update: " << newPath << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to delete existing photo: " << e.what() << std::endl;
        }
    }

    // Сохраняем новые данные фото
    std::vector<char> buffer(size);
    std::istringstream iss(data);
    iss.read(buffer.data(), size);

    std::ofstream outFile(newPath, std::ios::binary);
    if (outFile) {
        outFile.write(buffer.data(), size);
        outFile.close();
        std::cout << "Photo saved: " << newPath << std::endl;
        return Photo(newPath);
    }
    else {
        std::cerr << "Failed to open file for writing" << std::endl;
        return Photo();
    }
}