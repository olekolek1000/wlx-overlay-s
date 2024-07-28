#include "SDL_surface.h"
#include "app.hpp"
#include "fmt/format.h"
#include "handler.hpp"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_parser.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_types.h"
#include "include/internal/cef_types_wrappers.h"
#include "interface_class.hpp"
#include "logs.hpp"
#include "tab.hpp"
#include "util.hpp"
#include "whereami.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

extern "C" {
#include "interface.h"
}

static const uint32_t viewport_width = 1280;
static const uint32_t viewport_height = 720;

static const uint32_t navbar_width = viewport_width;
static const uint32_t navbar_height = 48;

static const uint32_t content_width = viewport_width;
static const uint32_t content_height = 720 - navbar_height;

static const bool debug_view_enabled = false;

Interface *interface = nullptr;

void setError(std::string_view str) { interface->error_msg = str; }

SDL_Surface *TabCell::getCompositeSurface(bool render) {
  if (!surf_composite) {
    surf_composite = SDL_CreateRGBSurfaceWithFormat(
        0, viewport_width, viewport_height, 0, SDL_PIXELFORMAT_ABGR8888);
  }

  if (render) {
    auto *surf_navbar = handler_navbar->getSurface();
    auto *surf_content = handler_content->getSurface();
    if (!surf_navbar || !surf_content) {
      setError("Handler surfaces not ready");
      return nullptr;
    }

    SDL_Rect rect{};

    // Blit navbar
    SDL_BlitSurface(surf_navbar, nullptr, surf_composite, &rect);

    // Blit content
    rect.y += surf_navbar->h;
    SDL_BlitSurface(surf_content, nullptr, surf_composite, &rect);
  }

  return surf_composite;
}

CefRefPtr<CefBrowser> TabCell::mapEvent(int x, int y, int &mapped_x,
                                        int &mapped_y) {
  last_mouse_x = x;
  last_mouse_y = y;
  if (y < (int)navbar_height) {
    mapped_x = x;
    mapped_y = y;
    return handler_navbar->browser;
  } else {
    mapped_x = x;
    mapped_y = y - (int)navbar_height;
    return handler_content->browser;
  }
}

void TabCell::passMouseMove(int x, int y) {
  int mapped_x, mapped_y;
  auto browser = mapEvent(x, y, mapped_x, mapped_y);
  if (!browser) [[unlikely]] {
    return;
  }

  if (auto host = browser->GetHost()) {
    CefMouseEvent evt;
    evt.x = mapped_x;
    evt.y = mapped_y;
    host->SendMouseMoveEvent(evt, false);
  }
}

void TabCell::passMouseState(int index, bool down) {
  int mapped_x, mapped_y;
  auto browser = mapEvent(last_mouse_x, last_mouse_y, mapped_x, mapped_y);
  if (!browser) [[unlikely]] {
    return;
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
    evt.x = mapped_x;
    evt.y = mapped_y;
    evt.modifiers = cef_event_flags_t::EVENTFLAG_LEFT_MOUSE_BUTTON;

    host->SendMouseClickEvent(evt, type, !down, 1);
  }
}

void TabCell::passMouseScroll(float delta) {
  int mapped_x, mapped_y;
  auto browser = mapEvent(last_mouse_x, last_mouse_y, mapped_x, mapped_y);
  if (!browser) [[unlikely]] {
    return;
  }

  if (auto host = browser->GetHost()) {
    CefMouseEvent evt;
    evt.x = last_mouse_x;
    evt.y = last_mouse_y;
    host->SendMouseWheelEvent(evt, 0, delta * 15);
  }
}

TabCell::~TabCell() {
  if (surf_composite) {
    SDL_FreeSurface(surf_composite);
    surf_composite = nullptr;
  }
}

int Interface::getUnusedHandlerId() const {
  int unused_id = 0;
  while (true) {
    if (tabs.find(unused_id) != tabs.end()) {
      unused_id++;
      continue;
    }
    break;
  }

  return unused_id;
}

CefSettings getSettings() {
  CefSettings settings;
  settings.log_severity = LOGSEVERITY_ERROR;
  settings.multi_threaded_message_loop = false;
  settings.windowless_rendering_enabled = true;
  settings.no_sandbox = true;
  CefString(&settings.browser_subprocess_path)
      .FromString(util::getLibraryPath());

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

  auto path = util::getLibraryPath();

  const char *argv[] = {path.c_str(), "--disable-gpu", "--no-sandbox",
                        "--single-process", nullptr};

  CefMainArgs main_args(4, (char **)argv);
  auto settings = getSettings();

  logs::print("Calling CefInitialize");
  CefInitialize(main_args, settings, interface->app.get(), nullptr);

  // this function is blocking, wlxcef_tick_message_loop is being used instead
  // CefRunMessageLoop();
}

void initApp() {
  logs::print("Allocating");
  interface->app = new App();
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

  logs::print("Calling CefExecuteProcess");
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

int wlxcef_set_url(int tab_id, const char *url) {
  auto handler = getHandler(tab_id);
  if (!handler) {
    return -1;
  }

  if (!handler->setURL(url)) {
    setError("setURL failed");
    return -1;
  }

  return 0;
}

int wlxcef_create_tab() {
  if (!interface->tabs.empty()) {
    setError("multiple handlers not supported yet");
    return -1;
  }

  const auto tab_id = interface->getUnusedHandlerId();

  initApp();

  logs::print("Creating handler ID {}", tab_id);

  CefRefPtr<Handler> handler_navbar(new Handler(
      interface->app.get(), navbar_width, navbar_height, debug_view_enabled));

  CefRefPtr<Handler> handler_content(new Handler(
      interface->app.get(), content_width, content_height, debug_view_enabled));
  handler_content->setCallbackAddressChange([handler_navbar](std::string url) {
    handler_navbar->callJavascriptFunction(fmt::format(
        "wlxcef_change_url(\"{}\")", CefURIEncode(url, true).ToString()));
  });

  interface->app->setHandlers(handler_navbar, handler_content);

  auto &cell = interface->tabs[tab_id];
  cell.handler_navbar = handler_navbar;
  cell.handler_content = handler_content;

  interface->startCef();

  return tab_id;
}

int wlxcef_is_ready(int tab_id) {
  wlxcef_tick_message_loop();
  auto handler = getHandler(tab_id);
  if (!handler) {
    return -1;
  }

  return handler->browser != nullptr;
}

int wlxcef_free_tab(int tab_id) {
  auto handler = getHandler(tab_id);
  if (!handler) {
    return -1;
  }

  interface->tabs.erase(interface->tabs.find(tab_id));
  return 0;
}

int wlxcef_get_viewport_width(int tab_id) {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return -1;
  }
  auto *surface = tab->getCompositeSurface(false);
  if (!surface) {
    return -1;
  }

  return surface->w;
}

int wlxcef_get_viewport_height(int tab_id) {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return -1;
  }
  auto *surface = tab->getCompositeSurface(false);
  if (!surface) {
    return -1;
  }

  return surface->h;
}

const void *wlxcef_get_viewport_data_rgba(int tab_id) {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return nullptr;
  }

  auto *surface = tab->getCompositeSurface(true);
  if (!surface) {
    return nullptr;
  }

  return surface->pixels;
}

int wlxcef_mouse_move(int tab_id, int x, int y) {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return -1;
  }

  tab->passMouseMove(x, y);
  return 0;
}

int wlxcef_mouse_set_state(int tab_id, int index, int down) {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return -1;
  }

  tab->passMouseState(index, down == 1);
  return 0;
}

int wlxcef_mouse_scroll(int tab_id, float delta) {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return -1;
  }

  tab->passMouseScroll(delta);
  return 0;
}
}