#pragma once
#include<iostream>
#include<sstream>
#include<string>
#include<vector>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <windows.h>

class Photo {
public:
    Photo(const std::string& photoPath = "")
        : m_photoPath(photoPath), m_size(0) {
        if (photoPath != "") {
            updateSize();
        }
    }

    Photo(const std::string& photoPath, size_t size)
        : m_photoPath(photoPath), m_size(size) {}

    std::string& getPhotoPath() { return m_photoPath; }
    void setPhotoPath(std::string& photoPath) { m_photoPath = photoPath; updateSize(); }
    std::size_t getSize() const { return m_size; }

    std::string serialize() const;
    static Photo deserialize(const std::string& data, size_t size, std::string oldLogin, std::string newLogin);

private:
    static std::string wideStringToString(const WCHAR* wideStr);
    void updateSize();

private:
    std::string m_photoPath;
    std::size_t m_size;
};

