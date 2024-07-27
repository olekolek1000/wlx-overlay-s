#include "app.hpp"
#include "handler.hpp"
#include "render_handler.hpp"
#include <cassert>
#include <filesystem>

App::App(std::string_view url) : url(url) {
  render_process_handler = new RenderProcessHandler();
}

CefRefPtr<CefBrowserProcessHandler> App::GetBrowserProcessHandler() {
  return this;
}

CefRefPtr<CefRenderProcessHandler> App::GetRenderProcessHandler() {
  assert(render_process_handler);
  return render_process_handler;
}

void App::OnContextInitialized() {
  assert(handler);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(0);

  CefString url = this->url;

  CefBrowserSettings settings;
  settings.windowless_frame_rate = 30;

  assert(handler);
  CefBrowserHost::CreateBrowser(window_info, handler, url, settings, {}, {});
}

CefRefPtr<CefClient> App::GetDefaultClient() { return handler; }

void App::setHandler(const CefRefPtr<Handler> &handler) {
  this->handler = handler;
}