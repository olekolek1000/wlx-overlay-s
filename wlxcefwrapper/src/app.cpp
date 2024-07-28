#include "app.hpp"
#include "handler.hpp"
#include "logs.hpp"
#include "render_handler.hpp"
#include "util.hpp"
#include <cassert>
#include <filesystem>

App::App() { render_process_handler = new RenderProcessHandler(); }

CefRefPtr<CefBrowserProcessHandler> App::GetBrowserProcessHandler() {
  return this;
}

CefRefPtr<CefRenderProcessHandler> App::GetRenderProcessHandler() {
  assert(render_process_handler);
  return render_process_handler;
}

void App::OnContextInitialized() {
  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);

  CefBrowserSettings settings;
  settings.windowless_frame_rate = 30;

  auto path = util::getLibraryDir() + "/../assets/index.html";

  CefBrowserHost::CreateBrowser(window_info, handler_navbar, "file://" + path,
                                settings, {}, {});

  CefBrowserHost::CreateBrowser(window_info, handler_content, "about:blank",
                                settings, {}, {});

  logs::print("Context and browser created");
}

CefRefPtr<CefClient> App::GetDefaultClient() {
  return nullptr;
  // return handler_content;
}

void App::setHandlers(const CefRefPtr<Handler> &handler_navbar,
                      const CefRefPtr<Handler> &handler_content) {
  this->handler_navbar = handler_navbar;
  this->handler_content = handler_content;
}