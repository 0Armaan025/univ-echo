#include "../include/univ-echo/database.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <cstdio>
#include <exception>

int main() {
  try {
    univecho::Database database("data/univecho.db");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::fprintf(stderr, "init failed (SDL): %s\n", SDL_GetError());
      return 1;
    }
    SDL_Window *window =
        SDL_CreateWindow("UNIV ECHO | Universal Memory", SDL_WINDOWPOS_CENTERED,
                         SDL_WINDOWPOS_CENTERED, 1100, 700, SDL_WINDOW_SHOWN);

    if (!window) {
      std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());

      SDL_Quit();
      return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
      std::fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());

      SDL_DestroyWindow(window);
      SDL_Quit();

      return 1;
    }

    bool running = true;
    SDL_Event event;

    while (running) {
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
          running = false;
        }

        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        }
      }

      // dark bg

      SDL_SetRenderDrawColor(renderer, 18, 19, 17, 255);
      SDL_RenderClear(renderer);

      SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
  }

  catch (const std::exception &error) {
    std::fprintf(stderr, "UNIVECHO ERROR: %s\n", error.what());
    return 1;
  }
}
