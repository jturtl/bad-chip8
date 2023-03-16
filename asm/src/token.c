#include "token.h"

#include <string.h> // strlen(), strcpy()
#include <ctype.h> // isalpha(), isdigit(), tolower()

#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>

#include <stdbool.h>

// This code is based on the Crafting Interpreters book, which uses Java.
// Forgive my six billion static variables, but I'm trying to emulate
// the Java class used in the book.
// Think of this token.c file as one big Class,
// and the static vars/functions are private!

static TokenList tl;
static const char *src;
static unsigned long long srcLen;

static bool hadError = false;

static unsigned long long start = 0, current = 0, line = 1;

#define TOKEN(t,s,n) ((Token){t,s,n,line})
#define BLANKTOKEN (TOKEN(0,"",0))

// C-Style string map.
typedef struct {
  char *str;
  TokenType type;
} TokenMapElem;

// !!!!!!!!!!!!!!
// REMEMBER TO UPDATE THIS YOU FRICKIN KNOB
// THAT WAS A REEEEALLY ANNOYING BUG
static const int tokenMapLen = 30;
// !!!!!!!!!!!!!

#define TM(s,t) ((TokenMapElem){s,t})
TokenMapElem tokenMap[] = {
  // non-standard commands
  TM("BREAK", TOKEN_BREAK),
  TM("PIXEL", TOKEN_PIXEL),

  TM("CLEAR", TOKEN_CLEAR),
  TM("JUMP", TOKEN_JUMP),
  TM("SUBROUTINE", TOKEN_SUBROUTINE),
  TM("RETURN", TOKEN_RETURN),
  TM("IFNEQ", TOKEN_IFNEQ),
  TM("IFEQ", TOKEN_IFEQ),
  TM("SET", TOKEN_SET),
  TM("ADD", TOKEN_ADD),
  TM("OR", TOKEN_OR),
  TM("AND", TOKEN_AND),
  TM("XOR", TOKEN_XOR),
  TM("SUB", TOKEN_SUB),
  TM("RSHIFT", TOKEN_RSHIFT),
  TM("REVSUB", TOKEN_REVSUB),
  TM("LSHIFT", TOKEN_LSHIFT),
  TM("RANDOM", TOKEN_RANDOM),
  TM("DRAW", TOKEN_DRAW),
  TM("IFNKEY", TOKEN_IFNKEY),
  TM("IFKEY", TOKEN_IFKEY),
  TM("AWAIT", TOKEN_AWAIT),
  TM("SETSPRITE", TOKEN_SETSPRITE),
  TM("BCD", TOKEN_BCD),
  TM("DUMP", TOKEN_DUMP),
  TM("FILL", TOKEN_FILL),
  TM("DATA", TOKEN_DATA),

  // not commands but still special keywords
  TM("I", TOKEN_INDEXREGISTER),
  TM("TIMER", TOKEN_TIMER),
  TM("SOUND", TOKEN_SOUND)
};
#undef TM

static void Error(const char *msg, ...) {

  va_list args;
  va_start(args, msg);

  printf("[ERROR, LINE %I64d] ", line);
  vfprintf(stderr, msg, args);
  printf("\n");

  va_end(args);

  hadError = true;
}

static bool IsAtEnd() {
  return current >= srcLen;
}

static void AddToken(TokenType type, const char *str, int num) {

  if (tl.len >= tl.cap-1) {
    tl.cap *= 2;
    tl.data = realloc(tl.data, sizeof(Token) * tl.cap);
  }

  tl.data[tl.len].type = type;
  tl.data[tl.len].numData = num;
  strcpy(tl.data[tl.len].strData, str);
  tl.data[tl.len].line = line;

  tl.len++;

}

static char Peek() {
  if (IsAtEnd()) return '\0';
  return src[current];
}

static char Advance() {
  current++;
  return src[current - 1];
}

static bool IsHexDigit(char c) {
  char a = toupper(c);
  if (a == 'A' || a == 'B' || a == 'C' || a == 'D' || a == 'E' || a == 'F') {
    return true;
  } else {
    return isdigit(c);
  }
}

static int ReadHexDigit(char c) {

  // well, it works.
  int ret;
  switch (c) {
    case '0': ret = 0; break;
    case '1': ret = 1; break;
    case '2': ret = 2; break;
    case '3': ret = 3; break;
    case '4': ret = 4; break;
    case '5': ret = 5; break;
    case '6': ret = 6; break;
    case '7': ret = 7; break;
    case '8': ret = 8; break;
    case '9': ret = 9; break;
    case 'A': ret = 10; break;
    case 'B': ret = 11; break;
    case 'C': ret = 12; break;
    case 'D': ret = 13; break;
    case 'E': ret = 14; break;
    case 'F': ret = 15; break;
    default:
      ret = 0;
      break;
  }

  return ret;
}

static Token HexNumber(bool *success) {
  while (IsHexDigit(Peek())) Advance();
  unsigned long long length = current-start;
  if (length > HEX_NUMBER_MAX_LENGTH) {
    Error("Error reading hexadecimal number: Too long!");
    *success = false;
    return BLANKTOKEN;
  }
  char str[64] = "";
  strncpy(str, src+start, current-start);


  Token tk;
  tk.type = TOKEN_NUMBER;
  tk.strData[0] = '$';
  strcpy(tk.strData+1, str);
  tk.line = line;

  // parse number
  tk.numData = 0;
  for (int i=0; i<strlen(str); i++) {
    tk.numData *= 16;
    tk.numData += ReadHexDigit(str[i]);
  }

  //printf("Scanned hexadecimal number '%s'\n", tk.strData);

  *success = true;
  return tk;
}

static Token DecNumber(bool *success) {
  while (isdigit(Peek())) Advance();
  int length = current-start;
  if (length > DEC_NUMBER_MAX_LENGTH) {
    Error("Error reading decimal number: Too long!");
    *success = false;
    return BLANKTOKEN;
  }
  char str[64] = "";
  strncpy(str, src+start, current-start);

  Token tk;
  tk.type = TOKEN_NUMBER;
  tk.strData[0] = '#';
  strcpy(tk.strData+1, str);
  tk.line = line;

  tk.numData = 0;
  for (int i=0; i<strlen(str); i++) {
    tk.numData *= 10;

    // ReadHexDigit still works for decimal
    // so why fix what ain't broke, eh?
    tk.numData += ReadHexDigit(str[i]);
  }

  //printf("Scanned decimal number '%s'\n", tk.strData);

  *success = true;
  return tk;
}

static bool IsBinDigit(char c) {
  return c == '1' || c == '0';
}

static Token BinNumber(bool *success) {
  while (IsBinDigit(Peek())) Advance();

  unsigned long long length = current-start;
  if (length > BIN_NUMBER_MAX_LENGTH) {
    Error("Error reading binary number: Too long!");
    *success = false;
    return BLANKTOKEN;
  }
  char str[64] = "";
  strncpy(str, src+start, current-start);

  Token tk;
  tk.type = TOKEN_NUMBER;

  tk.strData[0] = '%';
  strcpy(tk.strData+1, str);
  tk.line = line;

  tk.numData = 0;
  for (int i=0; i<strlen(str); i++) {
    tk.numData *= 2;
    tk.numData += ReadHexDigit(str[i]);
  }

  //printf("Scanned binary number '%s'\n", tk.strData);

  *success = true;
  return tk;
}

static Token Identifier(bool *success) {
  while (isalpha(Peek()) || isdigit(Peek())) {
    Advance();
  }

  unsigned long long length = current-start;
  if (length > IDENTIFIER_MAX_LENGTH) {
    Error("Error parsing identifier: Too long!");
    *success = false;
    return BLANKTOKEN;
  }

  char str[64] = "";
  strncpy(str, src+start, current-start);
  //printf("Scanned identifier '%s'\n", str);

  Token tk;

  // Whoever called this should know what they're doing
  // There is no generic 'identifier' type.
  tk.type = TOKEN_NONE;

  strcpy(tk.strData, str);
  tk.numData = 0;
  tk.line = line;

  *success = true;

  return tk;
}

static void ScanToken() {
  char c = Advance();

  switch(c) {

    case '$': {
      // Hexadecimal number
      // skip the '$' symbol
      start++;
      c = Advance();

      bool success;
      Token tk = HexNumber(&success);
      if (success)
        AddToken(tk.type, tk.strData, tk.numData);
      break;
    }


    case '#': {
      // Decimal number
      start++;
      c = Advance();

      bool success;
      Token tk = DecNumber(&success);
      if (success)
        AddToken(tk.type, tk.strData, tk.numData);
      break;
    }


    case '%': {
      // Binary number
      start++;
      c = Advance();

      bool success;
      Token tk = BinNumber(&success);
      if (success)
        AddToken(tk.type, tk.strData, tk.numData);
      break;
    }


    case ':': {
      // This is (supposedly) a label

      if (!isalpha(Peek())) {
        Error("Expected identifier after ':'\n");
      } else {
        bool success;
        Token tk = Identifier(&success);
        if (success) {
          tk.type = TOKEN_LABEL;
          // The ':' is still in tk.strData!
          AddToken(tk.type, tk.strData, tk.numData);
        }

      }
      break;
    }


    case ',': {
      AddToken(TOKEN_COMMA, ",", 0);
      break;
    }


    case ';': {
      // comment, ignore everything until end of the line (or program)
      while (Peek() != '\n' && !IsAtEnd()) {
        Advance();
      }
      break;
    }


    // ignore whitespace
    case ' ':
    case '\r':
    case '\t':
      break;

    // newlines are important though
    case '\n': {
      // avoid duplicates
      if (tl.data[tl.len-1].type != TOKEN_NEWLINE)
        AddToken(TOKEN_NEWLINE, "\\n", 0);
      line++;
      break;
    }


    default: {
      if (isalpha(c)) {
        bool success;
        Token tk = Identifier(&success);

        if (success) {

          // see if it's a built-in identifier
          for (int i=0; i<tokenMapLen; i++) {
            if (strcmp(tk.strData, tokenMap[i].str) == 0) {
              tk.type = tokenMap[i].type;
              break;
            }
          }

          if (tk.type == TOKEN_NONE) {
            // okay, it wasn't a built in command...
            // is it a register?
            if (strlen(tk.strData) == 2 && toupper(tk.strData[0]) == 'R') {
              if (IsHexDigit(tk.strData[1])) {
                tk.type = TOKEN_REGISTER;
                tk.numData = ReadHexDigit(tk.strData[1]);
                AddToken(tk.type, tk.strData, tk.numData);
              } else {
                Error("Invalid Register '%s'", tk.strData);
              }

            } else {
              Error("Unexpected identifier '%s'", tk.strData);
            }

          } else {
            // yeah it was a built in command, go on with your life
            AddToken(tk.type, tk.strData, tk.numData);
          }

        }

      // not isalpha(c)
      } else {
        Error("Unexpected character '%c'.", c);
      }
      break;
    }
  }
}

TokenList Tokenize(const char *program, bool *errorOccurred) {
  src = program;
  srcLen = strlen(src);

  tl.len = 0;
  tl.cap = 256;
  tl.data = malloc(sizeof(Token) * tl.cap);

  while (!IsAtEnd()) {
    start = current;
    ScanToken();
  }

  AddToken(TOKEN_EOF, "EOF", 0);

  *errorOccurred = hadError;

  return tl;
}

#undef BLANKTOKEN
