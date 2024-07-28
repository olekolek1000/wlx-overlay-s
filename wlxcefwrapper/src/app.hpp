#pragma once

#include "include/cef_app.h"
#include <string>

class Handler;

class App : public CefApp, public CefBrowserProcessHandler {
public:
  App();

  CefRefPtr<CefRenderProcessHandler> render_process_handler;

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;
  void OnContextInitialized() override;
  CefRefPtr<CefClient> GetDefaultClient() override;

  void setHandlers(const CefRefPtr<Handler> &handler_navbar,
                   const CefRefPtr<Handler> &handler_content);

private:
  CefRefPtr<Handler> handler_navbar;
  CefRefPtr<Handler> handler_content;

  IMPLEMENT_REFCOUNTING(App);
};
