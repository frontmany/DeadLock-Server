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

    std::ifstream file(m_photoPath, std::ios::binary);

    if (!file) {
        throw std::runtime_error("Failed to open file: " + m_photoPath);
    }

    std::vector<char> buffer(m_size);
    file.read(buffer.data(), m_size);
    file.close();

    return base64_encode(std::string(buffer.data(), m_size), false);
}


Photo Photo::deserialize(const std::string& data, size_t size, const std::string& login) {
    if (data.empty() || size == 0) {
        return Photo();
    }

    const std::string saveDirectory = "./ReceivedPhotos";
    try {
        std::filesystem::create_directories(saveDirectory);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
        return Photo();
    }

    const std::string path = saveDirectory + "/" + login + "Photo.png";

    if (std::filesystem::exists(path)) {
        try {
            std::filesystem::remove(path);
            std::cout << "Existing photo deleted: " << path << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to delete existing photo: " << e.what() << std::endl;
            return Photo();
        }
    }

    try {
        std::ofstream outFile(path, std::ios::binary);
        if (!outFile) {
            throw std::runtime_error("Failed to open file for writing");
        }

        outFile.write(data.data(), size);
        outFile.close();

        std::cout << "Photo saved successfully: " << path << std::endl;
        return Photo(path);

    }
    catch (const std::exception& e) {
        std::cerr << "Failed to save photo: " << e.what() << std::endl;
        return Photo();
    }
}