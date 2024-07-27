#pragma once

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include <SDL2/SDL.h>

enum class HandlerCommand { unrecoverableError };

CefRefPtr<CefProcessMessage> createMessage();

class App;

class Handler : public CefClient,
                public CefDisplayHandler,
                public CefLifeSpanHandler,
                public CefLoadHandler {
public:
  App *app;
  CefRefPtr<CefBrowser> browser{};
  int last_mouse_x = 0;
  int last_mouse_y = 0;

  explicit Handler(App *app, uint32_t viewport_w, uint32_t viewport_h,
                   bool debug_window);
  ~Handler();

  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
    return this;
  }
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
    return this;
  }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame, ErrorCode errorCode,
                           const CefString &errorText,
                           const CefString &failedUrl) override;

  void OnAddressChange(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                       const CefString &url) override;
  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
                        const CefString &message, const CefString &source,
                        int line) override;
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  void LoadEndPerform(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame);
  void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override;

  uint32_t getViewportWidth() const;
  uint32_t getViewportHeight() const;

  CefRefPtr<CefRenderHandler> GetRenderHandler() override;

  SDL_Surface *getViewportSurface();
  SDL_Surface *getViewportSurfaceRGBA();

  void updateSurface();

  void callJavascriptFunction(const CefRefPtr<CefBrowser> &browser,
                              const std::string &code);
  void triggerSiteLoadedEvent(const CefRefPtr<CefBrowser> &browser);

private:
  CefRefPtr<CefRenderHandler> render_handler;

  uint32_t viewport_w = 0, viewport_h = 0;

  void initSDL(bool debug_window);
  void freeSDL();

  SDL_Surface *viewport = nullptr;
  SDL_Surface *viewport_rgba = nullptr;
  SDL_Window *window = nullptr;

  IMPLEMENT_REFCOUNTING(Handler);
};
