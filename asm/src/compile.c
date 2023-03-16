#include "compile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static TokenList tokens;
static CompiledCode result;

static int address = CHIP8_START_ADDRESS;
static bool hadError = false;

static int current = 0;

typedef struct {
  char name[64];
  uint16_t address;
} Label;
static Label labels[COMPILE_MAX_LABELS];
static int nLabels = 0;

static Token tk;

typedef struct {
  int length;
  Token data[COMPILE_MAX_ARGS];
} ArgList;

static void Error(const char *msg, ...) {

  va_list args;
  va_start(args, msg);

  printf("[ERROR, LINE %ld] ", tk.line);
  vfprintf(stderr, msg, args);
  printf("\n");

  va_end(args);

  hadError = true;
}

static byte GetHiByte(uint16_t n) {
  return n >> 8;
}
static byte GetLoByte(uint16_t n) {
  return n & 0x00FF;
}

static Token Advance() {
  current++;
  return tokens.data[current];
}

static ArgList GetArgs() {
  ArgList ret;
  ret.length = 0;

  tk = Advance();

  while (tk.type != TOKEN_NEWLINE) {
    switch (tk.type) {
      case TOKEN_REGISTER:
      case TOKEN_INDEXREGISTER:
      case TOKEN_NUMBER:
      case TOKEN_LABEL:
      case TOKEN_TIMER:
      case TOKEN_SOUND: {
        if (ret.length >= COMPILE_MAX_ARGS) {
          Error("More than maximum (%d) arguments", COMPILE_MAX_ARGS);
          ret.length = 0;
          return ret;
        }
        ret.data[ret.length] = tk;
        ret.length++;
        break;
      }

      default: {
        Error("Unexpected '%s'", tk.strData);
        ret.length = 0;
        return ret;
      }
    }

    tk = Advance();
    if (tk.type == TOKEN_COMMA) {
      tk = Advance();
    } else if (tk.type != TOKEN_NEWLINE) {

      Error("Expected ',' but got '%s'", tk.strData);
      ret.length = 0;
      return ret;
    }
  }

  return ret;

}

static void AddLabel(const char *name, int address) {
  Label l;
  strcpy(l.name, name);
  l.address = address;

  // check for duplicates
  for (int i=0; i<nLabels; i++) {
    if (strcmp(name, labels[i].name) == 0) {
      Error("Duplicate label '%s'", name);
      return;
    }
  }

  labels[nLabels] = l;

  nLabels++;
}

static uint16_t GetLabelAddress(const char *name, bool *success) {

  for (int i=0; i<nLabels; i++) {
    if (strcmp(name, labels[i].name) == 0) {
      *success = true;
      return labels[i].address;
    }
  }

  *success = false;
  return 0;
}

static void Write(byte hi, byte lo) {
  result.data[address] = hi;
  result.data[address+1] = lo;
  address += 2;
}


static void TokenData() {
  ArgList args = GetArgs();

  if (args.length == 0) {
    Error("DATA cannot have zero arguments");
  } else {
    for (int i=0; i<args.length; i++) {
      if (args.data[i].type != TOKEN_NUMBER) {
        Error("Only numbers can be passed to DATA");
        break;
      } else if (args.data[i].numData > 255) {
        Error("Cannot pass numbers greater than 1 byte (255) to DATA");
        break;
      }
      result.data[address] = (byte)args.data[i].numData;
      address++;
    }
  }
}

static void TokenPixel() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("PIXEL requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];

    if (arg0.type != TOKEN_REGISTER || arg1.type != TOKEN_REGISTER) {
      Error("Both arguments to PIXEL must be registers");
    } else {
      // NON-STANDARD: Draw a pixel at (RX, RY)
      // Code: 5XY1
      byte hi, lo;
      hi = arg0.numData | 0x50;
      lo = (arg1.numData << 4) | 0x01;
      Write(hi, lo);
    }
  }
}

static void TokenLabel() {
  AddLabel(tk.strData, address);
}

static void TokenBreak() {
  ArgList args = GetArgs();
  if (args.length != 0) {
    Error("BREAK takes no arguments, but was given %d", args.length);
  } else {
    Write(0x00, 0x00);
  }
}

static void TokenClear() {
  ArgList args = GetArgs();
  if (args.length != 0) {
    Error("CLEAR takes no arguments, but was given %d", args.length);
  } else {
    Write(0x00, 0xE0);
  }
}

static void TokenReturn() {
  ArgList args = GetArgs();
  if (args.length != 0) {
    Error("RETURN takes no arguments, but was given %d", args.length);
  } else {
    Write(0x00, 0xEE);
  }
}

static void TokenJump() {

  ArgList args = GetArgs();

  if (args.length == 1) {
    if (args.data[0].type == TOKEN_LABEL) {
      bool success;
      uint16_t addr = GetLabelAddress(args.data[0].strData, &success);
      if (!success) {
        Error("Tried to jump to non existent label '%s'", args.data[0].strData);
      } else {
        // Jump to NNN
        // Code: 1NNN
        byte hi, lo;
        hi = GetHiByte(addr);
        hi |= 0x10;
        lo = GetLoByte(addr);
        Write(hi, lo);
      }
    } else if (args.data[0].type == TOKEN_NUMBER) {
      if (args.data[0].numData > 0xFFF) {
        Error("Cannot jump to address greater than 4095 (0xFFF), tried to jump to %d", args.data[0].numData);
      } else {
        // Jump to NNN
        // Code: 1NNN
        byte hi, lo;
        hi = GetHiByte(args.data[0].numData);
        hi |= 0x10;
        lo = GetLoByte(args.data[0].numData);
        Write(hi, lo);
      }
    } else {
      Error("Unexpected '%s'", args.data[0].strData);
    }

  } else if (args.length == 2) {
    bool ungabunga = true;
    uint16_t addr;
    if (args.data[0].type == TOKEN_LABEL) {
      bool success;
      addr = GetLabelAddress(args.data[0].strData, &success);
      if (!success) {
        Error("Tried to jump to non existent label '%s'", args.data[0].strData);
        ungabunga = false;
      }
    } else if (args.data[0].type == TOKEN_NUMBER) {
      if (args.data[0].numData > 0xFFF) {
        Error("Cannot jump to address greater than 4095 (0xFFF), tried to jump to %d", args.data[0].numData);
        ungabunga = false;
      } else {
        addr = args.data[0].numData;
      }
    } else {
      Error("Unexpected '%s'", args.data[0].strData);
      ungabunga = false;
    }

    if (ungabunga) {

      if (args.data[1].type == TOKEN_REGISTER) {
        if (args.data[1].numData != 0) {
          Error("Only Register 0 can be passed to JUMP offset");
          ungabunga = false;
        }
      } else {
        Error("Second argument to JUMP must be R0");
        ungabunga = false;
      }

      if (ungabunga) {
        // Jump to NNN, offset by R0
        // Code: BNNN
        byte hi, lo;
        hi = GetHiByte(addr);
        hi |= 0xB0;
        lo = GetLoByte(addr);
        Write(hi, lo);
      }

    }

  } else {
    Error("JUMP Requires 1 or 2 arguments, got %d", args.length);
  }

}

static void TokenSubroutine() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("SUBROUTINE requires 1 argument, got %d", args.length);
  } else {
    if (args.data[0].type == TOKEN_LABEL) {
      bool success;
      uint16_t addr = GetLabelAddress(args.data[0].strData, &success);
      if (!success) {
        Error("Called SUBROUTINE with non existent label '%s'", args.data[0].strData);
      } else {
        // Subroutine at NNN
        // Code: 2NNN
        byte hi, lo;
        hi = GetHiByte(addr);
        hi |= 0x20;
        lo = GetLoByte(addr);
        Write(hi, lo);
      }
    } else if (args.data[0].type == TOKEN_NUMBER) {
      if (args.data[0].numData > 4095) {
        Error("Cannot call subroutine at out of range address %04X", args.data[0].numData);
      } else {
        byte hi, lo;
        hi = GetHiByte(args.data[0].numData);
        hi |= 0x20;
        lo = GetLoByte(args.data[0].numData);
        Write(hi, lo);
      }
    } else {
      Error("Unexpected '%s'", args.data[0].strData);
    }
  }
}

static void TokenIFNEQ() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("IFNEQ requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];
    if (arg0.type != TOKEN_REGISTER) {
      Error("First argument to IFNEQ must be a register");
    } else {
      if (arg1.type == TOKEN_REGISTER) {
        // If Not Equal - Register/Register comparison
        // Code: 5XY0
        byte hi, lo;
        hi = arg0.numData;
        hi |= 0x50;
        lo = arg1.numData << 4;
        Write(hi, lo);
      } else if (arg1.type == TOKEN_NUMBER) {
        if (arg1.numData > 255) {
          Error("Cannot compare register to number greater than 1 byte");
        } else {
          // If Not Equal - Register/Number comparison
          // Code: 3XNN
          byte hi, lo;
          hi = arg0.numData;
          hi |= 0x30;
          lo = arg1.numData;
          Write(hi, lo);
        }
      } else {
        Error("Second argument to IFNEQ must be a register or 1 byte number");
      }
    }
  }
}

static void TokenIFEQ() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("IFEQ requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];
    if (arg0.type != TOKEN_REGISTER) {
      Error("First argument to IFEQ must be a register");
    } else {
      if (arg1.type == TOKEN_REGISTER) {
        // If Equal - Register/Register comparison
        // Code: 9XY0
        byte hi, lo;
        hi = arg0.numData;
        hi |= 0x90;
        lo = arg1.numData << 4;
        Write(hi, lo);
      } else if (arg1.type == TOKEN_NUMBER) {
        if (arg1.numData > 255) {
          Error("Cannot compare register to number greater than 1 byte");
        } else {
          // If Equal - Register/Number comparison
          // Code: 4XNN
          byte hi, lo;
          hi = arg0.numData;
          hi |= 0x40;
          lo = arg1.numData;
          Write(hi, lo);
        }
      } else {
        Error("Second argument to IFEQ must be a register or 1 byte number");
      }
    }
  }
}

static void TokenSet() {
  // warning: l a r g e

  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("SET requires 2 arguments (got %d)", args.length);
  } else {

    Token arg0 = args.data[0], arg1 = args.data[1];

    switch (arg0.type) {

      case TOKEN_NUMBER: {
        Error("Cannot SET a number");
        break;
      }

      case TOKEN_REGISTER: {
        switch (arg1.type) {
          case TOKEN_NUMBER: {
            if (arg1.numData > 255) {
              Error(
                "Registers can only hold 1 byte (0-255), tried to SET to '%d'",
                arg1.numData
              );
            } else {
              // SET Register to Number
              // Code: 6XNN
              byte hi, lo;
              lo = arg1.numData;
              hi = 0x60;
              hi += arg0.numData; // ID

              Write(hi, lo);
            }
            break;
          }

          case TOKEN_REGISTER: {
            // SET Register to Register
            // Code: 8XY0
            byte hi, lo;
            hi = 0x80;
            hi += arg0.numData;
            lo = arg1.numData << 4;

            Write(hi, lo);
            break;
          }

          case TOKEN_INDEXREGISTER: {
            Error("Register cannot be SET to Index Register");
            break;
          }

          case TOKEN_TIMER: {
            // SET Register to Timer
            // Code: FX07
            byte hi, lo;
            hi = arg0.numData | 0xF0;
            lo = 0x07;
            Write(hi, lo);
            break;
          }

          default: {
            Error("Register cannot be SET to '%s'", arg1.strData);
            break;
          }
        } // end switch (arg1.type)
        break;
      } // end case TOKEN_REGISTER

      case TOKEN_INDEXREGISTER: {
        switch (arg1.type) {
          case TOKEN_REGISTER: {
            Error(
              "Cannot directly SET Index Register to register\n"
              "    (Tip: SET I to 0, then ADD a register)"
            );
            break;
          }

          case TOKEN_INDEXREGISTER: {
            Error("Tried to SET Index Register to Index Register");
            break;
          }

          case TOKEN_NUMBER: {
            if (arg1.numData > 0xFFF) {
              Error("Index Register can only hold up to 4095, tried to SET to %d", arg1.numData);
            } else {
              // Set I to Number
              // Code: ANNN
              byte hi, lo;
              hi = GetHiByte(arg1.numData);
              hi |= 0xA0;
              lo = GetLoByte(arg1.numData);

              Write(hi, lo);
            }
            break;
          }

          case TOKEN_LABEL: {

            bool found;
            uint16_t addr = GetLabelAddress(arg1.strData, &found);

            if (!found) {
              Error("Tried to set Index Register to non-existent label '%s'", arg1.strData);
            } else {
              // Set I to label address
              // Code: ANNN
              byte hi, lo;
              hi = GetHiByte(addr);
              hi |= 0xA0;
              lo = GetLoByte(addr);

              Write(hi, lo);
            }

            break;
          }

          default: {
            Error("Cannot SET '%s' to Index Register");
            break;
          }
        }

        break;
      } // end case TOKEN_INDEXREGISTER

      case TOKEN_SOUND: {
        switch (arg1.type) {
          case TOKEN_REGISTER: {
            // Set SOUND to RX
            // Code: FX18
            byte hi, lo;
            hi = arg1.numData | 0xF0;
            lo = 0x18;
            Write(hi, lo);
            break;
          }

          case TOKEN_NUMBER: {
            Error(
              "Cannot directly SET the SOUND timer to a number\n"
              "  Tip: assign to a register first"
            );
            break;
          }

          default: {
            Error("Cannot SET the SOUND timer to '%s'", arg1.strData);
          }
        }
        break;
      }

      case TOKEN_TIMER: {
        switch (arg1.type) {
          case TOKEN_REGISTER: {
            // Set TIMER to RX
            // Code: FX15
            byte hi, lo;
            hi = arg1.numData | 0xF0;
            lo = 0x15;
            Write(hi, lo);
            break;
          }

          case TOKEN_NUMBER: {
            Error(
              "Cannot directly SET the TIMER to a number\n"
              "  Tip: assign to a register first"
            );
            break;
          }

          default: {
            Error("Cannot SET the TIMER to '%s'", arg1.strData);
          }
        }
        break;
      }

      default: {
        Error("Expected register, Index Register, TIMER or SOUND as first argument to SET");
        break;
      }
    } // end switch (arg0.type)

  } // end if (args.length == 2)
}

static void TokenAdd() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("ADD requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];

    if (arg0.type == TOKEN_REGISTER) {
      if (arg1.type == TOKEN_REGISTER) {
        // Add registers
        // Code: 8XY4
        byte hi, lo;
        hi = arg0.numData;
        hi |= 0x80;
        lo = arg1.numData << 4;
        lo |= 0x04;
        Write(hi, lo);
      } else if (arg1.type == TOKEN_NUMBER) {
        if (arg1.numData > 255) {
          Error("Cannot ADD number greater than 1 byte to a register");
        } else {
          // Add number to register
          // Code: 7XNN
          byte hi, lo;
          hi = arg0.numData;
          hi |= 0x70;
          lo = arg1.numData;
          Write(hi, lo);
        }
      } else if (arg1.type == TOKEN_INDEXREGISTER) {
        Error("Cannot add Index Register to a register");
      } else {
        Error("Invalid '%s' as second argument to ADD", arg1.strData);
      }
    } else if (arg0.type == TOKEN_INDEXREGISTER) {
      if (arg1.type == TOKEN_REGISTER) {
        // Add register to I
        // Code: FX1E
        byte hi, lo;
        hi = arg1.numData;
        hi |= 0xF0;
        lo = 0x1E;

        Write(hi, lo);
      } else {
        Error(
          "Cannot directly ADD number to I\n"
          "  Tip: Assign a value to a register, then ADD it to I"
        );
      }
    } else {
      Error("Invalid '%s' as first argument to ADD", arg0.strData);
    }
  }
}

static void TokenSub() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("SUB requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];

    if (arg0.type != TOKEN_REGISTER || arg1.type != TOKEN_REGISTER) {
      Error("Both arguments to SUB must be registers");
    } else {
      // Subtract register from register
      // Code: 8XY5
      byte hi, lo;
      hi = arg0.numData | 0x80;
      lo = (arg1.numData << 4) | 0x05;
      Write(hi, lo);
    }
  }
}

static void TokenRevsub() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("REVSUB requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];

    if (arg0.type != TOKEN_REGISTER || arg1.type != TOKEN_REGISTER) {
      Error("Both arguments to REVSUB must be registers");
    } else {
      // Set RX to RY - RX
      // Code: 8XY7
      byte hi, lo;
      hi = arg0.numData | 0x80;
      lo = (arg1.numData << 4) | 0x07;
      Write(hi, lo);
    }
  }
}

static void TokenAnd() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("AND requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];

    if (arg0.type != TOKEN_REGISTER || arg1.type != TOKEN_REGISTER) {
      Error("Both arguments to AND must be registers");
    } else {
      // Bitwise AND registers
      // Code: 8XY2
      byte hi, lo;
      hi = arg0.numData | 0x80;
      lo = (arg1.numData << 4) | 0x02;
      Write(hi, lo);
    }
  }
}

static void TokenOr() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("OR requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];

    if (arg0.type != TOKEN_REGISTER || arg1.type != TOKEN_REGISTER) {
      Error("Both arguments to OR must be registers");
    } else {
      // Bitwise OR registers
      // Code: 8XY1
      byte hi, lo;
      hi = arg0.numData | 0x80;
      lo = (arg1.numData << 4) | 0x01;
      Write(hi, lo);
    }
  }
}

static void TokenXor() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("XOR requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];

    if (arg0.type != TOKEN_REGISTER || arg1.type != TOKEN_REGISTER) {
      Error("Both arguments to XOR must be registers");
    } else {
      // Bitwise XOR registers
      // Code: 8XY3
      byte hi, lo;
      hi = arg0.numData | 0x80;
      lo = (arg1.numData << 4) | 0x03;
      Write(hi, lo);
    }
  }
}

static void TokenRshift() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("RSHIFT requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];

    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to RSHIFT must be a register");
    } else {
      // Right shift Register
      // Code: 8XY6 (Y unused)
      byte hi, lo;
      hi = arg.numData | 0x80;
      lo = 0x06;
      Write(hi, lo);
    }
  }
}

static void TokenLshift() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("LSHIFT requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];

    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to LSHIFT must be a register");
    } else {
      // Left shift Register
      // Code: 8XYE (Y unused)
      byte hi, lo;
      hi = arg.numData | 0x80;
      lo = 0x0E;
      Write(hi, lo);
    }
  }
}

static void TokenRandom() {
  ArgList args = GetArgs();

  if (args.length != 2) {
    Error("RANDOM requires 2 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0];
    Token arg1 = args.data[1];

    if (arg0.type != TOKEN_REGISTER) {
      Error("First argument to RANDOM must be a register");
    } else if (arg1.type != TOKEN_NUMBER) {
      Error("Second argument to RANDOM must be a number");
    } else if (arg1.numData > 255) {
      Error("Second argument to RANDOM out of range (must be 1 byte)");
    } else {
      // Set Register to a random number 0-255 ANDed by NN
      // Code: CXNN
      byte hi, lo;
      hi = arg0.numData | 0xC0;
      lo = arg1.numData;
      Write(hi, lo);
    }
  }
}

static void TokenDraw() {
  ArgList args = GetArgs();

  if (args.length != 3) {
    Error("DRAW requires 3 arguments, got %d", args.length);
  } else {
    Token arg0 = args.data[0], arg1 = args.data[1], arg2 = args.data[2];
    if (arg0.type != TOKEN_REGISTER || arg1.type != TOKEN_REGISTER) {
      Error("First 2 arguments to DRAW must be registers");
    } else if (arg2.type != TOKEN_NUMBER) {
      Error("Third argument to DRAW must be a number");
    } else if (arg2.numData > 15) {
      Error("Third argument to DRAW must be in range 0-15");
    } else {
      // Draw sprite at RX,RY, width = 8, height = N
      // Code: DXYN
      byte hi, lo;
      hi = arg0.numData | 0xD0;
      lo = (arg1.numData << 4) | arg2.numData;
      Write(hi, lo);
    }
  }
}

static void TokenSetsprite() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("SETSPRITE requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];
    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to SETSPRITE must be a register");
    } else {
      // Set I to location of Hex Char sprite in RX
      // Code: FX29
      byte hi, lo;
      hi = arg.numData | 0xF0;
      lo = 0x29;
      Write(hi, lo);
    }
  }
}

static void TokenIFNKEY() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("IFNKEY requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];
    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to IFNKEY must be a register");
    } else {
      // Skip next instruction if register's key is pressed
      // Code: EX9E
      byte hi, lo;
      hi = arg.numData | 0xE0;
      lo = 0x9E;
      Write(hi, lo);
    }
  }
}

static void TokenIFKEY() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("IFKEY requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];
    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to IFKEY must be a register");
    } else {
      // Skip next instruction if register's key isn't pressed
      // Code: EXA1
      byte hi, lo;
      hi = arg.numData | 0xE0;
      lo = 0xA1;
      Write(hi, lo);
    }
  }
}

static void TokenAwait() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("AWAIT requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];
    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to AWAIT must be a register");
    } else {
      // Stop execution until a key is pressed, put it in the register
      // Code: FX0A
      byte hi, lo;
      hi = arg.numData | 0xF0;
      lo = 0x0A;
      Write(hi, lo);
    }
  }
}

static void TokenBCD() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("BCD requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];
    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to BCD must be a register");
    } else {
      // Store the BCD representation of register at I
      // Code: FX33
      byte hi, lo;
      hi = arg.numData | 0xF0;
      lo = 0x33;
      Write(hi, lo);
    }
  }
}

static void TokenDump() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("DUMP requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];
    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to DUMP must be a register");
    } else {
      // Dump registers R0->RX at I
      // Code: FX55
      byte hi, lo;
      hi = arg.numData | 0xF0;
      lo = 0x55;
      Write(hi, lo);
    }
  }
}

static void TokenFill() {
  ArgList args = GetArgs();

  if (args.length != 1) {
    Error("FILL requires 1 argument, got %d", args.length);
  } else {
    Token arg = args.data[0];
    if (arg.type != TOKEN_REGISTER) {
      Error("Argument to FILL must be a register");
    } else {
      // Fill registers R0->RX at I
      // Code: FX65
      byte hi, lo;
      hi = arg.numData | 0xF0;
      lo = 0x65;
      Write(hi, lo);
    }
  }
}

static void GetLabels() {
  while (tk.type != TOKEN_EOF) {
    switch (tk.type) {
      case TOKEN_LABEL: TokenLabel(); break;

      case TOKEN_NEWLINE: /* huh. */ break;

      case TOKEN_DATA: {
        address += GetArgs().length;
        break;
      }

      default: {
        GetArgs();
        address += 2;
        break;
      }
    }

    if (hadError) break;
    tk = Advance();
  }
}

CompiledCode Compile(TokenList tl, bool *errorOccurred) {
  tokens = tl;

  result.length = CHIP8_MEMORY_SIZE - CHIP8_START_ADDRESS;
  result.data = malloc(CHIP8_MEMORY_SIZE - CHIP8_START_ADDRESS);

  address = CHIP8_START_ADDRESS;
  current = 0;
  tk = tokens.data[current];

  GetLabels();

  if (hadError) {
    *errorOccurred = true;
    return result;
  }

  address = 0;
  current = 0;
  tk = tokens.data[current];

  while (tk.type != TOKEN_EOF) {
    switch (tk.type) {
      case TOKEN_DATA:        TokenData();       break;

      case TOKEN_LABEL:       /* Skip! */      break;

      case TOKEN_BREAK:       TokenBreak();      break;
      case TOKEN_CLEAR:       TokenClear();      break;
      case TOKEN_RETURN:      TokenReturn();     break;

      case TOKEN_JUMP:        TokenJump();       break;
      case TOKEN_SUBROUTINE:  TokenSubroutine(); break;

      case TOKEN_IFNEQ:       TokenIFNEQ();      break;
      case TOKEN_IFEQ:        TokenIFEQ();       break;

      case TOKEN_SET:         TokenSet();        break;
      case TOKEN_ADD:         TokenAdd();        break;
      case TOKEN_SUB:         TokenSub();        break;
      case TOKEN_REVSUB:      TokenRevsub();     break;
      case TOKEN_AND:         TokenAnd();        break;
      case TOKEN_OR:          TokenOr();         break;
      case TOKEN_XOR:         TokenXor();        break;
      case TOKEN_RSHIFT:      TokenRshift();     break;
      case TOKEN_LSHIFT:      TokenLshift();     break;

      case TOKEN_RANDOM:      TokenRandom();     break;

      case TOKEN_PIXEL:       TokenPixel();      break;
      case TOKEN_DRAW:        TokenDraw();       break;
      case TOKEN_SETSPRITE:   TokenSetsprite();  break;

      case TOKEN_IFNKEY:      TokenIFNKEY();     break;
      case TOKEN_IFKEY:       TokenIFKEY();      break;
      case TOKEN_AWAIT:       TokenAwait();      break;

      case TOKEN_BCD:         TokenBCD();        break;

      case TOKEN_DUMP:        TokenDump();       break;
      case TOKEN_FILL:        TokenFill();       break;

      case TOKEN_NEWLINE:      /* :) */          break;

      default: {
        Error("Unexpected '%s'", tk.strData);
        break;
      }

    }

    if (hadError) break;
    tk = Advance();
  }

  result.length = address;
  result.data = realloc(result.data, result.length);

  *errorOccurred = hadError;

  return result;
}
