#pragma once

#include "app.hpp"
#include "tab.hpp"
#include <string>
#include <unordered_map>

struct Interface {
  std::string error_msg;
  CefRefPtr<App> app;
  bool cef_started = false;

  std::unordered_map<int, TabCell> tabs;
  int getUnusedHandlerId() const;
  void startCef();
};

extern Interface *interface;
extern void setError(std::string_view str);