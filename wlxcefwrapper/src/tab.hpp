#pragma once

#include "handler.hpp"

struct TabCell {
  CefRefPtr<Handler> handler_navbar;
  CefRefPtr<Handler> handler_content;
  int last_mouse_x = 0;
  int last_mouse_y = 0;

  SDL_Surface *surf_composite = nullptr;

  CefRefPtr<CefBrowser> mapEvent(int x, int y, int &mapped_x, int &mapped_y);
  SDL_Surface *getCompositeSurface(bool render);
  void passMouseMove(int x, int y);
  void passMouseState(int index, bool down);
  void passMouseScroll(float delta);
  ~TabCell();
};

TabCell *getTabCell(int tab_id);

CefRefPtr<Handler> getHandler(int tab_id);

CefRefPtr<CefBrowser> getBrowser(CefRefPtr<Handler> handler);

CefRefPtr<CefBrowser> getBrowser(int tab_id);