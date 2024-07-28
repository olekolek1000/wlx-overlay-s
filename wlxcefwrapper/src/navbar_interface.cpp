#include "navbar_interface.hpp"
#include "tab.hpp"

namespace navbar_interface {
// FIXME: no multi-window support yet, passing 0 as id
int tab_id = 0;

void nav_back() {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return;
  }

  tab->handler_content->callJavascriptFunction("window.history.go(-1)");
}

void nav_forward() {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return;
  }

  tab->handler_content->callJavascriptFunction("window.history.go(1)");
}

void nav_refresh() {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return;
  }

  tab->handler_content->callJavascriptFunction("window.location.reload()");
}

void nav_set_url(const char *url) {
  auto tab = getTabCell(tab_id);
  if (!tab) {
    return;
  }
  tab->handler_content->setURL(url);
}
} // namespace navbar_interface