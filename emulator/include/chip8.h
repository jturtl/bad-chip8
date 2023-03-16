#pragma once

#include "typedefs.h"

#define CHIP8_MEM_SIZE 4096
#define CHIP8_PROGRAM_START_ADDRESS 0x200
#define CHIP8_MAX_PROGRAM_SIZE (CHIP8_MEM_SIZE - CHIP8_PROGRAM_START_ADDRESS)
#define CHIP8_STACK_SIZE 16

#define CHIP8_SCREEN_W 64
#define CHIP8_SCREEN_H 32

typedef struct {
  byte *mem;
  byte R[16];
  word I;
  word SP;
  word stack[CHIP8_STACK_SIZE];
  word PC;

  byte timer, sound;

  bool screen[CHIP8_SCREEN_W][CHIP8_SCREEN_H];

  bool keys[16];

  bool waserror;
  char errormsg[256]; // error message

  bool quit;
} chip8;

bool chip8_init(chip8*);
void chip8_quit(chip8*);

bool chip8_loadrom(chip8*, byte *rom, long length);

byte chip8_read(chip8*, word addr);
void chip8_write(chip8*, word addr, byte value);

void chip8_dump_registers(chip8*);

void chip8_step(chip8*);

void chip8_timer_tick(chip8*);
