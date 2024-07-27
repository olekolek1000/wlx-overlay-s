#include "app.hpp"
#include "handler.hpp"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"
#include "include/internal/cef_types_wrappers.h"
#include "whereami.h"
#include <cassert>
#include <filesystem>
#include <queue>
#include <string>
#include <unordered_map>

extern "C" {
#include "interface.h"
}

static const uint32_t viewport_width = 1280;
static const uint32_t viewport_height = 720;
static const bool debug_view_enabled = false;

struct HandlerCell {
  CefRefPtr<Handler> handler;

  void init(CefRefPtr<App> &app);
};

struct Interface {
  std::string error_msg;
  CefRefPtr<App> app;
  bool cef_started = false;

  std::unordered_map<int, HandlerCell> handlers;
  int getUnusedHandlerId() const;
  void startCef();
};

Interface *interface = nullptr;

static void setError(std::string_view str) { interface->error_msg = str; }

void HandlerCell::init(CefRefPtr<App> &app) {}

int Interface::getUnusedHandlerId() const {
  int unused_id = 0;
  while (true) {
    if (handlers.find(unused_id) != handlers.end()) {
      unused_id++;
      continue;
    }
    break;
  }

  return unused_id;
}

std::string getExecutablePath() {
  std::string exec_path;
  auto exec_path_len = wai_getModulePath(nullptr, 0, nullptr);
  exec_path.resize(exec_path_len);
  int dirname_length;
  if (wai_getModulePath(exec_path.data(), exec_path_len, &dirname_length) ==
      -1) {
    fprintf(stderr, "Failed to get library exec path");
    abort();
  }
  // printf("Lib path: %s\n", exec_path.c_str());
  // fflush(stdout);
  return exec_path;
}

CefSettings getSettings() {
  CefSettings settings;
  settings.log_severity = LOGSEVERITY_ERROR;
  settings.multi_threaded_message_loop = false;
  settings.windowless_rendering_enabled = true;
  settings.no_sandbox = true;
  CefString(&settings.browser_subprocess_path).FromString(getExecutablePath());

  const auto cache_path = std::filesystem::absolute("./cef_cache").string();
  std::filesystem::create_directories(cache_path);
  CefString(&settings.cache_path).FromString(cache_path);

  return settings;
}

void Interface::startCef() {
  if (cef_started) {
    return;
  }
  cef_started = true;

  auto path = getExecutablePath();

  const char *argv[] = {path.c_str(), "--no-sandbox", "--disable-gpu",
                        "--single-process", nullptr};

  CefMainArgs main_args(4, (char **)argv);
  auto settings = getSettings();

  fprintf(stderr, "Initializing CEF\n");
  fflush(stderr);
  CefInitialize(main_args, settings, interface->app.get(), nullptr);

  fprintf(stderr, "Running CEF message loop\n");
  fflush(stderr);

  // this function is blocking, wlxcef_tick_message_loop is being used instead
  // CefRunMessageLoop();
}

void initApp() {
  fprintf(stderr, "Starting app\n");
  std::string_view url = "https://github.com/galister/wlx-overlay-s";
  interface->app = new App(url);
}

CefRefPtr<Handler> getHandler(int id) {
  auto it = interface->handlers.find(id);
  if (it == interface->handlers.end()) {
    setError("Handler not found");
    return {};
  }

  return it->second.handler;
}

CefRefPtr<CefBrowser> getBrowser(CefRefPtr<Handler> handler) {
  if (!handler || !handler->browser) {
    setError("Browser is not yet set");
    return {};
  }

  return handler->browser;
}

CefRefPtr<CefBrowser> getBrowser(int handler_id) {
  auto handler = getHandler(handler_id);
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

extern "C" {

int main(int argc, char **argv) {
  fprintf(stderr, "args: {}");
  for (int i = 0; i < argc; i++) {
    fprintf(stderr, "%s ", argv[i]);
  }
  fflush(stderr);

  CefMainArgs main_args(argc, argv);

  initApp();

  fprintf(stderr, "Executing process\n");
  int exit_code = CefExecuteProcess(main_args, interface->app.get(), nullptr);
  if (exit_code >= 0) {
    return exit_code;
  }
}

void wlxcef_tick_message_loop() { CefDoMessageLoopWork(); }

const char *wlxcef_get_error() { return interface->error_msg.c_str(); }

int wlxcef_init() {
  if (interface) {
    setError("wlxcef_init was already called");
    return -1;
  }

  interface = new Interface();

  return 0;
}
int wlxcef_free() {
  if (interface) {
    // CefShutdown();

    delete interface;
    interface = nullptr;
  }

  return 0;
}

int wlxcef_create_tab() {
  if (!interface->handlers.empty()) {
    setError("multiple handlers not supported yet");
    return -1;
  }

  const auto handler_id = interface->getUnusedHandlerId();

  initApp();

  fprintf(stderr, "Creating handler ID %d\n", handler_id);
  fflush(stderr);
  CefRefPtr<Handler> handler(new Handler(interface->app.get(), viewport_width,
                                         viewport_height, debug_view_enabled));
  interface->app->setHandler(handler);

  auto &cell = interface->handlers[handler_id];
  cell.handler = handler;

  interface->startCef();

  return handler_id;
}

int wlxcef_free_tab(int handler_id) {
  auto handler = getHandler(handler_id);
  if (!handler) {
    return -1;
  }

  interface->handlers.erase(interface->handlers.find(handler_id));
  return 0;
}

int wlxcef_get_viewport_width(int handler_id) {
  auto handler = getHandler(handler_id);
  if (!handler) {
    return -1;
  }
  return handler->getViewportWidth();
}
int wlxcef_get_viewport_height(int handler_id) {
  auto handler = getHandler(handler_id);
  if (!handler) {
    return -1;
  }
  return handler->getViewportHeight();
}
const void *wlxcef_get_viewport_data_rgba(int handler_id) {
  auto handler = getHandler(handler_id);
  if (!handler) {
    return nullptr;
  }

  auto *surf = handler->getViewportSurfaceRGBA();
  if (!surf) {
    setError("Surface is not yet available");
    return nullptr;
  }
  return surf->pixels;
}

int wlxcef_mouse_move(int handler_id, int x, int y) {
  auto handler = getHandler(handler_id);
  if (!handler) {
    return -1;
  }

  auto browser = getBrowser(handler);
  if (!browser) {
    return -1;
  }

  if (auto host = browser->GetHost()) {
    CefMouseEvent evt;
    evt.x = x;
    evt.y = y;
    handler->last_mouse_x = x;
    handler->last_mouse_y = y;
    host->SendMouseMoveEvent(evt, false);
  }

  return 0;
}

int wlxcef_mouse_set_state(int handler_id, int index, int down) {
  auto handler = getHandler(handler_id);
  if (!handler) {
    return -1;
  }

  auto browser = getBrowser(handler);
  if (!browser) {
    return -1;
  }

  cef_mouse_button_type_t type;
  switch (index) {
  default: {
    type = cef_mouse_button_type_t::MBT_LEFT;
    break;
  }
  case 1: {
    type = cef_mouse_button_type_t::MBT_MIDDLE;
    break;
  }
  case 2: {
    type = cef_mouse_button_type_t::MBT_RIGHT;
    break;
  }
  }

  if (auto host = browser->GetHost()) {
    CefMouseEvent evt;
    evt.x = handler->last_mouse_x;
    evt.y = handler->last_mouse_y;
    evt.modifiers = cef_event_flags_t::EVENTFLAG_LEFT_MOUSE_BUTTON;

    host->SendMouseClickEvent(evt, type, !down, 1);
  }

  return 0;
}

int wlxcef_mouse_scroll(int handler_id, float delta) {
  auto handler = getHandler(handler_id);
  if (!handler) {
    return -1;
  }

  auto browser = getBrowser(handler);
  if (!browser) {
    return -1;
  }

  if (auto host = browser->GetHost()) {
    CefMouseEvent evt;
    evt.x = handler->last_mouse_x;
    evt.y = handler->last_mouse_y;
    host->SendMouseWheelEvent(evt, 0, delta * 10);
  }

  return 0;
}
}