#pragma once

#include "typedefs.h"

#include "token.h"

#define CHIP8_START_ADDRESS 0x200
#define CHIP8_MEMORY_SIZE 0x1000
#define CHIP8_MAX_PROGRAM_SIZE (CHIP8_MEMORY_SIZE - CHIP8_START_ADDRESS)
#define COMPILE_MAX_LABELS 1024
#define COMPILE_MAX_ARGS 8

typedef struct {
  byte *data;
  int length;
} CompiledCode;

CompiledCode Compile(TokenList tl, bool *errorOccurred);
