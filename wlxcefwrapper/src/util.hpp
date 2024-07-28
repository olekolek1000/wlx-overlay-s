#pragma once

#include <string>
namespace util {
std::string getDataURI(const std::string &data, const std::string &mime_type);

std::string sanitizeInputString(std::string in);

std::string getLibraryPath();
std::string getLibraryDir();
} // namespace util