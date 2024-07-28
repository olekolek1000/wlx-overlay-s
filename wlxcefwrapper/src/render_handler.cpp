#include "render_handler.hpp"
#include "app.hpp"
#include "handler.hpp"
#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"
#include "include/base/cef_ref_counted.h"
#include "include/internal/cef_ptr.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "logs.hpp"
#include "navbar_interface.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>

bool V8Handler::Execute(const CefString &name, CefRefPtr<CefV8Value> object,
                        const CefV8ValueList &arguments,
                        CefRefPtr<CefV8Value> &retval, CefString &exception) {
  createMessage();

  auto func_name = name.ToString();
  if (func_name == "wlxcefpost_nav_back") {
    navbar_interface::nav_back();
  } else if (func_name == "wlxcefpost_nav_forward") {
    navbar_interface::nav_forward();
  } else if (func_name == "wlxcefpost_nav_refresh") {
    navbar_interface::nav_refresh();
  }

  logs::print("executed! {}", name.ToString());
  return false;
}

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
  if (type != PET_VIEW) {
    return;
  }

  auto *surface = handler->getSurface();
  if (!surface) {
    return;
  }

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
  }
}

void RenderProcessHandler::sendCommand(CefRefPtr<CefBrowser> browser,
                                       HandlerCommand command,
                                       std::string opt_data) {
  auto msg = createMessage();
  auto args = msg->GetArgumentList();
  args->SetInt(0, (int)command);
  if (!opt_data.empty()) {
    args->SetString(1, opt_data);
  }
  browser->GetMainFrame()->SendProcessMessage(PID_BROWSER, msg);
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
    break;
  }
  }

  handleException(browser, exception);

  return false;
}

void RenderProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                            CefRefPtr<CefFrame> frame,
                                            CefRefPtr<CefV8Context> context) {
  auto global = context->GetGlobal();
  auto v8_handler = new V8Handler();

  auto add = [&](const char *func_name) {
    global->SetValue(func_name,
                     CefV8Value::CreateFunction(func_name, v8_handler),
                     V8_PROPERTY_ATTRIBUTE_NONE);
  };

  add("wlxcefpost_url_change");
  add("wlxcefpost_nav_back");
  add("wlxcefpost_nav_forward");
  add("wlxcefpost_nav_refresh");
}
