#include "handler.hpp"
#include "logs.hpp"
#include <SDL2/SDL_video.h>
#include <signal.h>

uint32_t Handler::getViewportWidth() const { return viewport_w; }

uint32_t Handler::getViewportHeight() const { return viewport_h; }

void Handler::initSDL(bool debug_window) {
  if (debug_window) {
    window =
        SDL_CreateWindow("CEF", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         getViewportWidth(), getViewportHeight(), 0);
  }

  auto render_handler = GetRenderHandler();
  auto frame_handler = GetFrameHandler();

  logs::print("SDL initialized");
}

void Handler::freeSDL() {
  SDL_DestroyWindow(window);
  window = nullptr;

  SDL_FreeSurface(surf);
  surf = nullptr;
}

SDL_Surface *Handler::getSurface() {
  if (!surf) {
    surf = SDL_CreateRGBSurfaceWithFormat(0, getViewportWidth(),
                                          getViewportHeight(), 0,
                                          SDL_PIXELFORMAT_ARGB8888);
  }

  return surf;
}

bool Handler::setURL(const char *url) {
  if (!browser) {
    return false;
  }
  browser->GetMainFrame()->LoadURL(url);
  return true;
}

void Handler::updateSurface() {
  if (window && surf) {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
      if (evt.type == SDL_QUIT) {
        raise(SIGINT);
      }
    }

    auto *window_surface = SDL_GetWindowSurface(window);

    SDL_BlitSurface(surf, nullptr, window_surface, nullptr);

    SDL_UpdateWindowSurface(window);
  }
}

CefRefPtr<CefRenderHandler> Handler::GetRenderHandler() {
  return render_handler;
}
