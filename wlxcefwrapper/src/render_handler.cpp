#include "render_handler.hpp"
#include "app.hpp"
#include "handler.hpp"
#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"
#include "include/base/cef_ref_counted.h"
#include "include/internal/cef_ptr.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include <cstdio>
#include <fstream>

RenderHandler::RenderHandler(Handler *handler) : handler(handler) {}

RenderHandler::~RenderHandler() {}

void RenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect &rect) {
  rect.x = 0;
  rect.y = 0;
  rect.width = handler->getViewportWidth();
  rect.height = handler->getViewportHeight();
}

void RenderHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                            PaintElementType type, const RectList &dirtyRects,
                            const void *buffer, int width, int height) {
  CEF_REQUIRE_UI_THREAD();
  if (type != PET_VIEW)
    return;

  auto *surface = handler->getViewportSurface();
  SDL_LockSurface(surface);

  memcpy(surface->pixels, buffer, surface->pitch * surface->h);

  SDL_UnlockSurface(surface);

  handler->updateSurface();
}

void RenderProcessHandler::handleException(
    CefRefPtr<CefBrowser> browser, const CefRefPtr<CefV8Exception> &exception) {
  if (exception) {
    auto str = exception->GetMessage().ToString();
    auto msg = createMessage();
    auto args = msg->GetArgumentList();
    args->SetInt(0, (int)HandlerCommand::unrecoverableError);
    args->SetString(1, str);
    browser->GetMainFrame()->SendProcessMessage(PID_BROWSER, msg);
    throw std::runtime_error(str);
  }
}

bool RenderProcessHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    CefProcessId source_process, CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_RENDERER_THREAD();
  auto list = message->GetArgumentList();
  auto cmd = (RendererCommand)list->GetInt(0);

  CefRefPtr<CefV8Value> retval;
  CefRefPtr<CefV8Exception> exception;

  auto v8 = frame->GetV8Context();

  switch (cmd) {
  case RendererCommand::evalJavascript: {
    auto code = list->GetString(1);
    printf("Evaluating Javascript: %s\n", code.ToString().c_str());
    v8->Eval(code, frame->GetURL(), 0, retval, exception);
    break;
  }
  case RendererCommand::siteLoaded: {
    printf("Website loaded\n");
    break;
  }
  }

  handleException(browser, exception);

  return false;
}

void RenderProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                            CefRefPtr<CefFrame> frame,
                                            CefRefPtr<CefV8Context> context) {}
