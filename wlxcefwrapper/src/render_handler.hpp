#pragma once

#include "include/cef_render_handler.h"
#include "include/cef_render_process_handler.h"

enum struct RendererCommand { evalJavascript, siteLoaded };

class Handler;

class RenderProcessHandler : public CefRenderProcessHandler {
public:
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;
  void handleException(CefRefPtr<CefBrowser> browser,
                       const CefRefPtr<CefV8Exception> &exception);

  IMPLEMENT_REFCOUNTING(RenderProcessHandler);
};

class RenderHandler : public CefRenderHandler {
  Handler *handler;

public:
  RenderHandler(Handler *handler);
  ~RenderHandler();

  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) override;
  void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
               const RectList &dirtyRects, const void *buffer, int width,
               int height) override;

  IMPLEMENT_REFCOUNTING(RenderHandler);
};
