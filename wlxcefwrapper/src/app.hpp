#pragma once

#include "include/cef_app.h"
#include <string>

class Handler;

class App : public CefApp, public CefBrowserProcessHandler {
public:
  App(std::string_view url);

  std::string url;

  CefRefPtr<CefRenderProcessHandler> render_process_handler;

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;
  void OnContextInitialized() override;
  CefRefPtr<CefClient> GetDefaultClient() override;

  void setHandler(const CefRefPtr<Handler> &handler);

private:
  CefRefPtr<Handler> handler;

  IMPLEMENT_REFCOUNTING(App);
};
