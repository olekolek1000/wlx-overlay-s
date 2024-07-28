#include "util.hpp"
#include "include/cef_parser.h"
#include "logs.hpp"
#include "whereami.h"
#include <string>

namespace util {
std::string getDataURI(const std::string &data, const std::string &mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}

static std::string_view getDirectoryFromPath(std::string_view input_path) {
  auto pos = input_path.find_last_of('/');
  if (pos != std::string::npos) {
    return input_path.substr(0, pos);
  }
  return input_path;
}

std::string getLibraryPath() {
  std::string exec_path;
  auto exec_path_len = wai_getModulePath(nullptr, 0, nullptr);
  exec_path.resize(exec_path_len);
  int dirname_length;
  if (wai_getModulePath(exec_path.data(), exec_path_len, &dirname_length) ==
      -1) {
    logs::print("Failed to get library path");
    abort();
  }
  // logs::print("Lib path: {}", exec_path);
  return exec_path;
}

std::string getLibraryDir() {
  return std::string(getDirectoryFromPath(getLibraryPath()));
}

} // namespace util