#include "tab.hpp"
#include "interface_class.hpp"

TabCell *getTabCell(int tab_id) {
  auto it = interface->tabs.find(tab_id);
  if (it == interface->tabs.end()) {
    setError("Tab not found");
    return nullptr;
  };

  return &it->second;
}

CefRefPtr<Handler> getHandler(int tab_id) {
  auto it = interface->tabs.find(tab_id);
  if (it == interface->tabs.end()) {
    setError("Handler not found");
    return {};
  }

  return it->second.handler_content;
}

CefRefPtr<CefBrowser> getBrowser(CefRefPtr<Handler> handler) {
  if (!handler || !handler->browser) {
    setError("Browser is not yet set");
    return {};
  }

  return handler->browser;
}

CefRefPtr<CefBrowser> getBrowser(int tab_id) {
  auto handler = getHandler(tab_id);
  if (!handler) {
    return {};
  }

  auto browser = handler->browser;
  if (!browser) {
    setError("Browser is not yet set");
    return {};
  }

  return browser;
}