#include "handler.hpp"
#include "app.hpp"
#include "fmt/format.h"
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
#include "logs.hpp"
#include "navbar_interface.hpp"
#include "render_handler.hpp"
#include "util.hpp"
#include <chrono>
#include <thread>

Handler::Handler(App *app, uint32_t viewport_w, uint32_t viewport_h,
                 bool debug_window)
    : app(app) {

  this->viewport_w = viewport_w;
  this->viewport_h = viewport_h;

  render_handler = new RenderHandler(this);

  initSDL(debug_window);
}

Handler::~Handler() { freeSDL(); }

void Handler::setCallbackAddressChange(
    std::function<void(std::string)> &&callback) {
  this->callback_address_change = std::move(callback);
}

void Handler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  logs::print("afterCreated called");
  this->browser = browser;
}

static bool replaceString(std::string &str, const std::string &from,
                          const std::string &to) {
  size_t start_pos = str.find(from);
  if (start_pos == std::string::npos) {
    return false;
  }
  str.replace(start_pos, from.size(), to);
  return true;
}

void Handler::OnLoadError(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame, ErrorCode errorCode,
                          const CefString &errorText,
                          const CefString &failedUrl) {
  CEF_REQUIRE_UI_THREAD();

  if (errorCode == ERR_ABORTED) {
    return;
  }

  std::string str = R"STR(
<!DOCTYPE html>
<html>
	<head>
		<title>Webpage load error</title>
		<style>
			body {
				font-family: system-ui;
				background-color: #333;
        align-text: center;
        display: flex;
        justify-content: center;
        align-items: center;
        flex-direction: column;
			}
			h2 {
				color: #CCC;
			}
      h3 {
        color: #AAA;
      }
      h4 {
        color: #888;
      }
		</style>
	</head>
	<body>
		<h2>
      Load Error
    </h2>
    <h3>
      {content}
    </h3>
    <h4>
      {error}
    </h4>
	</body>
</html>
)STR";

  replaceString(str, "{content}",
                fmt::format("Failed to load URL {}", failedUrl.ToString()));

  replaceString(str, "{error}", fmt::format("Error: {}", errorText.ToString()));

  frame->LoadURL(util::getDataURI(str, "text/html"));
}

void Handler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame, const CefString &url) {
  auto url_str = url.ToString();
  logs::print("Address changed to {}", url_str.c_str());
  if (callback_address_change) {
    callback_address_change(url_str);
  }
}

bool Handler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                               cef_log_severity_t level,
                               const CefString &message,
                               const CefString &source, int line) {
  logs::print("Console message: {}", message.ToString());
  return false;
}

bool Handler::OnBeforePopup(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    const CefString &target_url, const CefString &target_frame_name,
    WindowOpenDisposition target_disposition, bool user_gesture,
    const CefPopupFeatures &popupFeatures, CefWindowInfo &windowInfo,
    CefRefPtr<CefClient> &client, CefBrowserSettings &settings,
    CefRefPtr<CefDictionaryValue> &extra_info, bool *no_javascript_access) {
  if (!target_url.empty()) {
    setURL(target_url.ToString().c_str());
  }

  // Prevent pop-up, return true because we want to have a custom action
  return true;
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
    auto str = list->GetString(1);
    logs::print("JS exception: {}", str.ToString().c_str());
    break;
  }
  case HandlerCommand::nav_back: {
    navbar_interface::nav_back();
    break;
  }
  case HandlerCommand::nav_forward: {
    navbar_interface::nav_forward();
    break;
  }
  case HandlerCommand::nav_refresh: {
    navbar_interface::nav_refresh();
    break;
  }
  case HandlerCommand::nav_new_window: {
    logs::print("new_window TODO");
    break;
  }
  case HandlerCommand::nav_set_url: {
    if (list->GetSize() >= 1) {
      auto str = list->GetString(1);
      navbar_interface::nav_set_url(str.ToString().c_str());
    }
    break;
  } break;
  }
  return false;
}

CefRefPtr<CefProcessMessage> createMessage() {
  auto msg = CefProcessMessage::Create("msg");
  return msg;
}

void Handler::callJavascriptFunction(const std::string &code) {
  if (!browser)
    return;
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
  logs::print("Webpage load ended, status code {}", httpStatusCode);

  CEF_REQUIRE_UI_THREAD();

  CefPostDelayedTask(
      TID_UI, base::BindOnce(&Handler::LoadEndPerform, this, browser, frame),
      1000);
}
