#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#include "chip8.h"

byte *load_rom_from_file(const char *path, long *len) {

  FILE *fp = fopen(path, "rb");

  if (fp == NULL) {
    *len = 0;
    return NULL;
  }

  fseek(fp, 0, SEEK_END);
  long length = ftell(fp);
  rewind(fp);

  byte *buffer = malloc(length);

  if (buffer == NULL) {
    fclose(fp);
    *len = 0;
    return NULL;
  }

  size_t result = fread(buffer, sizeof(byte), (size_t)length, fp);

  if (result != length) {
    free(buffer);
    fclose(fp);
    *len = 0;
    return NULL;
  }

  fclose(fp);

  *len = length;

  return buffer;
}


int main(int argc, char *argv[]) {

  chip8 ch8;
  if (!chip8_init(&ch8)) {
    printf("Failed to start Chip-8\n");
    return 1;
  }

  long length = 44;
  byte *rom = load_rom_from_file("out.ch8rom", &length);
  if (rom == NULL) {
    printf("Failed to read ROM 'out.ch8rom'\n");
    free(rom);
    chip8_quit(&ch8);
    return 1;
  } else if (length > CHIP8_MAX_PROGRAM_SIZE) {
    printf("ROM too large: 'out.ch8rom'\n");
    free(rom);
    chip8_quit(&ch8);
    return 1;
  }

  if (!chip8_loadrom(&ch8, rom, length)) {
    printf("%s\n", ch8.errormsg);
  }
  free(rom);

  SDL_Init(SDL_INIT_VIDEO);

  SDL_Window *window = SDL_CreateWindow(
    "CHIP-8",
    SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
    512, 256,
    SDL_WINDOW_SHOWN
  );

  SDL_Renderer *renderer = SDL_CreateRenderer(
    window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
  );

  // for (int i=0x200; i<0x200+length; i+=2) {
  //   printf("[%04X] %02X%02X\n", i, ch8.mem[i], ch8.mem[i+1]);
  // }

  SDL_Texture *screen = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_RGBA8888,
    SDL_TEXTUREACCESS_TARGET,
    CHIP8_SCREEN_W, CHIP8_SCREEN_H
  );

  if (screen == NULL) {
    printf("Failed to create screen texture\n");
    chip8_quit(&ch8);
    return 1;
  }

  uint32_t next_timer_update = 0;
  uint32_t next_screen_update = 0;
  uint32_t next_chip8_step = 0;

  while (!ch8.quit) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT: {
          ch8.quit = true;
          break;
        }

        case SDL_KEYDOWN: {
          switch (event.key.keysym.sym) {
            case SDLK_1: ch8.keys[0x1] = true; break;
            case SDLK_2: ch8.keys[0x2] = true; break;
            case SDLK_3: ch8.keys[0x3] = true; break;
            case SDLK_q: ch8.keys[0x4] = true; break;
            case SDLK_w: ch8.keys[0x5] = true; break;
            case SDLK_e: ch8.keys[0x6] = true; break;
            case SDLK_a: ch8.keys[0x7] = true; break;
            case SDLK_s: ch8.keys[0x8] = true; break;
            case SDLK_d: ch8.keys[0x9] = true; break;

            case SDLK_UP: ch8.keys[0x2] = true; break;
            case SDLK_DOWN: ch8.keys[0x8] = true; break;
            case SDLK_LEFT: ch8.keys[0x4] = true; break;
            case SDLK_RIGHT: ch8.keys[0x6] = true; break;
          }
          break;
        }

        case SDL_KEYUP: {
          switch (event.key.keysym.sym) {
            case SDLK_1: ch8.keys[0x1] = false; break;
            case SDLK_2: ch8.keys[0x2] = false; break;
            case SDLK_3: ch8.keys[0x3] = false; break;
            case SDLK_q: ch8.keys[0x4] = false; break;
            case SDLK_w: ch8.keys[0x5] = false; break;
            case SDLK_e: ch8.keys[0x6] = false; break;
            case SDLK_a: ch8.keys[0x7] = false; break;
            case SDLK_s: ch8.keys[0x8] = false; break;
            case SDLK_d: ch8.keys[0x9] = false; break;

            case SDLK_UP: ch8.keys[0x2] = false; break;
            case SDLK_DOWN: ch8.keys[0x8] = false; break;
            case SDLK_LEFT: ch8.keys[0x4] = false; break;
            case SDLK_RIGHT: ch8.keys[0x6] = false; break;
          }
          break;
        }
      }
    }

    if (SDL_GetTicks() >= next_timer_update) {
      chip8_timer_tick(&ch8);
      next_timer_update = SDL_GetTicks() + 16;
    }

    if (SDL_GetTicks() >= next_chip8_step) {
      //printf("[%04X] %02X%02X\n", ch8.PC, ch8.mem[ch8.PC], ch8.mem[ch8.PC+1]);
      chip8_step(&ch8);
      next_chip8_step = SDL_GetTicks() + 0;
    }


    if (SDL_GetTicks() >= next_screen_update) {

      SDL_SetRenderTarget(renderer, screen);
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      for (int i=0; i<CHIP8_SCREEN_W; i++) {
        for (int j=0; j<CHIP8_SCREEN_H; j++) {
          if (ch8.screen[i][j]) {
            SDL_RenderDrawPoint(renderer, i, j);
          }
        }
      }

      SDL_SetRenderTarget(renderer, NULL);
      SDL_RenderCopy(renderer, screen, NULL, NULL);
      SDL_RenderPresent(renderer);

      next_screen_update = SDL_GetTicks() + 32;
    }

    //SDL_Delay(1);
  }

  if (ch8.waserror) {
    printf("CHIP-8 ERROR: %s\n", ch8.errormsg);
  }

  chip8_quit(&ch8);
  SDL_Quit();

  return 0;
}
