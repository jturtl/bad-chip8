#pragma once

#include <stdbool.h>

#define BIN_NUMBER_MAX_LENGTH 16
#define DEC_NUMBER_MAX_LENGTH 5
#define HEX_NUMBER_MAX_LENGTH 4
#define IDENTIFIER_MAX_LENGTH 63

typedef enum {
  TOKEN_NONE,
  TOKEN_REGISTER,
  TOKEN_INDEXREGISTER,
  TOKEN_NUMBER,
  TOKEN_LABEL,
  TOKEN_TIMER,
  TOKEN_SOUND,

  TOKEN_DATA,
  TOKEN_PIXEL, // non-standard!

  TOKEN_COMMA,
  TOKEN_NEWLINE,

  TOKEN_BREAK, // also non-standard!
  TOKEN_CLEAR,
  TOKEN_RETURN,
  TOKEN_JUMP,
  TOKEN_SUBROUTINE,
  TOKEN_IFNEQ,
  TOKEN_IFEQ,
  TOKEN_SET,
  TOKEN_ADD,
  TOKEN_OR,
  TOKEN_AND,
  TOKEN_XOR,
  TOKEN_SUB,
  TOKEN_RSHIFT,
  TOKEN_REVSUB,
  TOKEN_LSHIFT,
  TOKEN_RANDOM,
  TOKEN_DRAW,
  TOKEN_IFNKEY,
  TOKEN_IFKEY,
  TOKEN_AWAIT,
  TOKEN_SETSPRITE,
  TOKEN_BCD,
  TOKEN_DUMP,
  TOKEN_FILL,

  TOKEN_EOF,
  TOKEN_MAX
} TokenType;

typedef struct {
  TokenType type;
  char strData[64];
  unsigned int numData;

  unsigned long line;
} Token;

typedef struct {
  Token *data;
  unsigned long len, cap;
} TokenList;

TokenList Tokenize(const char *src, bool *errorOccurred);
