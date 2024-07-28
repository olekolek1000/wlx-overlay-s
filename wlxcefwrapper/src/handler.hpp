#pragma once

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_request_handler.h"
#include "include/wrapper/cef_resource_manager.h"
#include <SDL2/SDL.h>
#include <functional>

enum class HandlerCommand {
  nav_back,
  nav_forward,
  nav_refresh,
  nav_new_window,
  nav_set_url,
  unrecoverableError
};

CefRefPtr<CefProcessMessage> createMessage();

class App;

class Handler : public CefClient,
                public CefDisplayHandler,
                public CefLifeSpanHandler,
                public CefResourceRequestHandler,
                public CefLoadHandler {
public:
  App *app;
  CefRefPtr<CefBrowser> browser{};

  explicit Handler(App *app, uint32_t viewport_w, uint32_t viewport_h,
                   bool debug_window);
  ~Handler();

  void setCallbackAddressChange(std::function<void(std::string)> &&callback);

  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
    return this;
  }
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
    return this;
  }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

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

  bool OnBeforePopup(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                     const CefString &target_url,
                     const CefString &target_frame_name,
                     WindowOpenDisposition target_disposition,
                     bool user_gesture, const CefPopupFeatures &popupFeatures,
                     CefWindowInfo &windowInfo, CefRefPtr<CefClient> &client,
                     CefBrowserSettings &settings,
                     CefRefPtr<CefDictionaryValue> &extra_info,
                     bool *no_javascript_access) override;

  void LoadEndPerform(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame);
  void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override;

  uint32_t getViewportWidth() const;
  uint32_t getViewportHeight() const;

  CefRefPtr<CefRenderHandler> GetRenderHandler() override;

  SDL_Surface *getSurface();

  void updateSurface();

  bool setURL(const char *url);

  void callJavascriptFunction(const std::string &code);

  void triggerSiteLoadedEvent(const CefRefPtr<CefBrowser> &browser);

private:
  CefRefPtr<CefRenderHandler> render_handler;

  uint32_t viewport_w = 0, viewport_h = 0;

  void initSDL(bool debug_window);
  void freeSDL();

  SDL_Surface *surf = nullptr;
  SDL_Window *window = nullptr;

  std::function<void(std::string)> callback_address_change;

  IMPLEMENT_REFCOUNTING(Handler);
};
