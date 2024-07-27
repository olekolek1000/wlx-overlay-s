#include "handler.hpp"
#include <SDL2/SDL_video.h>
#include <signal.h>

uint32_t Handler::getViewportWidth() const { return viewport_w; }

uint32_t Handler::getViewportHeight() const { return viewport_h; }

void Handler::initSDL(bool debug_window) {
  viewport = SDL_CreateRGBSurfaceWithFormat(
      0, getViewportWidth(), getViewportHeight(), 0, SDL_PIXELFORMAT_RGB888);

  if (debug_window) {
    window =
        SDL_CreateWindow("CEF", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         getViewportWidth(), getViewportHeight(), 0);
  }

  auto render_handler = GetRenderHandler();
  auto frame_handler = GetFrameHandler();

  fprintf(stderr, "SDL initialized\n");
}

void Handler::freeSDL() {
  SDL_DestroyWindow(window);
  window = nullptr;

  SDL_FreeSurface(viewport);
  viewport = nullptr;

  SDL_FreeSurface(viewport_rgba);
  viewport_rgba = nullptr;
}

SDL_Surface *Handler::getViewportSurface() { return viewport; }

SDL_Surface *Handler::getViewportSurfaceRGBA() { return viewport_rgba; }

void Handler::updateSurface() {
  if (window) {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
      if (evt.type == SDL_QUIT) {
        raise(SIGINT);
      }
    }

    auto *window_surface = SDL_GetWindowSurface(window);

    SDL_BlitSurface(viewport, nullptr, window_surface, nullptr);

    SDL_UpdateWindowSurface(window);
  }

  if (!viewport_rgba) {
    viewport_rgba = SDL_CreateRGBSurfaceWithFormat(0, getViewportWidth(),
                                                   getViewportHeight(), 0,
                                                   SDL_PIXELFORMAT_ABGR8888);
  }

  SDL_BlitSurface(viewport, nullptr, viewport_rgba, nullptr);
}

CefRefPtr<CefRenderHandler> Handler::GetRenderHandler() {
  return render_handler;
}
