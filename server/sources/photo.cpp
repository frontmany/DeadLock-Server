#include "photo.h"
#include "base64.h"
#include <locale>
#include <codecvt>

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


#ifdef _WIN32
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wide_path = converter.from_bytes(m_photoPath);
    std::ifstream file(wide_path, std::ios::binary);
#else
    std::ifstream file(m_photoPath, std::ios::binary);
#endif

    if (!file) {
        throw std::runtime_error("Failed to open file: " + m_photoPath);
    }

    std::vector<char> buffer(m_size);
    file.read(buffer.data(), m_size);
    file.close();

    return base64_encode(std::string(buffer.data(), m_size), false);
}

Photo Photo::deserialize(const std::string& data, size_t size, std::string oldLogin, std::string newLogin) {
    // ≈сли данных о фото нет - возвращаем пустое фото
    if (data.empty()) {
        return Photo();
    }
    
    std::string saveDirectory = ":/ReceivedFiles";
    std::filesystem::create_directories(saveDirectory);

    std::string newPath = saveDirectory + "/" + newLogin + "Photo.png";
    std::string oldPath = saveDirectory + "/" + oldLogin + "Photo.png";

    if (newLogin != oldLogin) {
        // ”дал€ем старое фото, если оно существует
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
    

    // ”дал€ем существующее фото с новым логином (если есть)
    if (std::filesystem::exists(newPath)) {
        try {
            std::filesystem::remove(newPath);
            std::cout << "Existing photo deleted for update: " << newPath << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to delete existing photo: " << e.what() << std::endl;
        }
    }

    // —охран€ем новые данные фото
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