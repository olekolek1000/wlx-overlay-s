#include "handler.hpp"
#include "app.hpp"
#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/cef_parser.h"
#include "include/cef_task.h"
#include "include/internal/cef_ptr.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "render_handler.hpp"
#include <chrono>
#include <thread>

namespace {
std::string GetDataURI(const std::string &data, const std::string &mime_type) {
  return "data:" + mime_type + ";base64," +
         CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
             .ToString();
}
} // namespace

Handler::Handler(App *app, uint32_t viewport_w, uint32_t viewport_h,
                 bool debug_window)
    : app(app) {

  this->viewport_w = viewport_w;
  this->viewport_h = viewport_h;

  render_handler = new RenderHandler(this);

  initSDL(debug_window);
}

Handler::~Handler() { freeSDL(); }

void Handler::OnLoadError(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame, ErrorCode errorCode,
                          const CefString &errorText,
                          const CefString &failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  if (errorCode == ERR_ABORTED) {
    return;
  }

  std::stringstream ss;
  ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
     << std::string(failedUrl) << " with error " << std::string(errorText)
     << " (" << errorCode << ").</h2></body></html>";

  frame->LoadURL(GetDataURI(ss.str(), "text/html"));
}

void Handler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame, const CefString &url) {
  fprintf(stderr, "Address changed to %s\n", url.ToString().c_str());
  this->browser = browser;
}

bool Handler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                               cef_log_severity_t level,
                               const CefString &message,
                               const CefString &source, int line) {
  return false;
}

namespace {
auto timer_start = std::chrono::high_resolution_clock::now();
}

uint64_t getMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::high_resolution_clock::now() - timer_start)
      .count();
}

bool Handler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       CefProcessId source_process,
                                       CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();

  auto list = message->GetArgumentList();
  auto cmd = (HandlerCommand)list->GetInt(0);

  switch (cmd) {
  case HandlerCommand::unrecoverableError: {
    char *out = nullptr;
    auto str = list->GetString(1);
    asprintf(&out, "exception: %s", str.ToString().c_str());
    fprintf(stdout, "%s\n", out);
    fflush(stderr);
    fflush(stdout);
    free(out);
    break;
  }
  }
  return false;
}

CefRefPtr<CefProcessMessage> createMessage() {
  auto msg = CefProcessMessage::Create("msg");
  return msg;
}

void Handler::callJavascriptFunction(const CefRefPtr<CefBrowser> &browser,
                                     const std::string &code) {
  auto msg = createMessage();
  auto args = msg->GetArgumentList();
  args->SetInt(0, (int)RendererCommand::evalJavascript);
  args->SetString(1, code);
  browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, msg);
}

void Handler::triggerSiteLoadedEvent(const CefRefPtr<CefBrowser> &browser) {
  auto msg = createMessage();
  auto args = msg->GetArgumentList();
  args->SetInt(0, (int)RendererCommand::siteLoaded);
  browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, msg);
}

void Handler::LoadEndPerform(CefRefPtr<CefBrowser> browser,
                             CefRefPtr<CefFrame> frame) {
  CEF_REQUIRE_UI_THREAD();

  triggerSiteLoadedEvent(browser);
}

void Handler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame, int httpStatusCode) {
  fprintf(stderr, "Webpage load ended, status code %d\n", httpStatusCode);

  CEF_REQUIRE_UI_THREAD();

  CefPostDelayedTask(
      TID_UI, base::BindOnce(&Handler::LoadEndPerform, this, browser, frame),
      1000);
}
