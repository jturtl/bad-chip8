#include "chip8.h"

#include "hexdata.h"

#include <stdlib.h>
#include <string.h>

static void chip8_error(chip8 *ch8, const char *msg) {
  strcpy(ch8->errormsg, msg);
  ch8->waserror = true;
  ch8->quit = true;
}

bool chip8_init(chip8 *ch8) {
  ch8->mem = malloc(CHIP8_MEM_SIZE);
  if (ch8->mem == NULL) {
    chip8_error(ch8, "Failed to allocate memory");
    return false;
  }

  // hexadecimal number sprites
  memmove(ch8->mem, HEXDATA, 80);

  for (int i=0; i<16; i++) {
    ch8->R[i] = 0;
  }

  ch8->I = 0;
  ch8->SP = 0;
  ch8->PC = CHIP8_PROGRAM_START_ADDRESS;

  ch8->timer = 0;
  ch8->sound = 0;

  for (int i=0; i<CHIP8_SCREEN_W; i++) {
    for (int j=0; j<CHIP8_SCREEN_H; j++) {
      ch8->screen[i][j] = false;
    }
  }

  for (int i=0; i<16; i++) {
    ch8->keys[i] = false;
  }

  ch8->quit = false;

  ch8->waserror = false;

  return true;
}

void chip8_quit(chip8 *ch8) {
  free(ch8->mem);
}

bool chip8_loadrom(chip8 *ch8, byte *rom, long length) {
  if (length < CHIP8_MEM_SIZE - CHIP8_PROGRAM_START_ADDRESS) {
    memmove(ch8->mem+CHIP8_PROGRAM_START_ADDRESS, rom, length);
    return true;
  } else {
    chip8_error(ch8, "Loaded ROM is too big");
    return false;
  }
}

byte chip8_read(chip8 *ch8, word addr) {
  if (addr >= CHIP8_MEM_SIZE) {
    chip8_error(ch8, "Out-of-bounds memory read");
    return 0;
  } else {
    return ch8->mem[addr];
  }
}

void chip8_write(chip8 *ch8, word addr, byte value) {
  if (addr >= CHIP8_MEM_SIZE) {
    chip8_error(ch8, "Out-of-bounds memory write");
  } else {
    ch8->mem[addr] = value;
  }
}

static void chip8_advance(chip8 *ch8) {
  ch8->PC += 2;
}

static void chip8_clear(chip8 *ch8) {
  for (int i=0; i<CHIP8_SCREEN_W; i++) {
    for (int j=0; j<CHIP8_SCREEN_H; j++) {
      ch8->screen[i][j] = false;
    }
  }
}

static void chip8_jump(chip8 *ch8, word destination) {
  ch8->PC = destination;
}

static void chip8_subroutine(chip8 *ch8, word destination) {
  ch8->stack[ch8->SP] = ch8->PC;
  ch8->SP++;
  ch8->PC = destination;
}

static void chip8_return(chip8 *ch8) {
  if (ch8->SP == 0) {
    chip8_error(ch8, "Stack underflow");
  } else if (ch8->SP >= CHIP8_STACK_SIZE) {
    chip8_error(ch8, "Stack overflow");
  } else {
    ch8->SP--;
    ch8->PC = ch8->stack[ch8->SP];
    chip8_advance(ch8);
  }
}

static void chip8_pixel(chip8 *ch8, byte x, byte y) {
  if (x < CHIP8_SCREEN_W && y < CHIP8_SCREEN_H)
    ch8->screen[x][y] = !ch8->screen[x][y];
}

static void chip8_draw(chip8 *ch8, byte x, byte y, byte height) {

  word addr = ch8->I;

  bool bits[8];
  for (int i=0; i<height; i++) {
    for (int b=7; b>=0; b--) {
      bits[b] = (chip8_read(ch8, addr+i) & (128>>b)) != 0;
    }

    for (int j=0; j<8; j++) {
      if (x+j < CHIP8_SCREEN_W && y+i < CHIP8_SCREEN_H)
        ch8->screen[x+j][y+i] ^= bits[j];
    }
  }

}

void chip8_timer_tick(chip8 *ch8) {
  if (ch8->timer > 0) ch8->timer--;
  if (ch8->sound > 0) ch8->sound--;
}

void chip8_step(chip8 *ch8) {

  // get current instruction
  byte high, low;
  high = chip8_read(ch8, ch8->PC);
  low = chip8_read(ch8, (ch8->PC)+1);
  // the whole 2-byte instruction
  word w = (high << 8) | low;

  // common instruction parts
  // Register X: 0X00
  byte x = high & 0x0F;
  // Register Y: 00Y0
  byte y = (low & 0xF0) >> 4;
  // Destination (or other 1.5 byte value): 0NNN
  word destination = w & 0x0FFF;

  byte firstnibble = (high & 0xF0) >> 4;
  byte lastnibble = (low & 0x0F);
  switch (firstnibble) {

    case 0x0: {

      if (w == 0x0000) {
        // 0000 NON-STANDARD: BREAK
        ch8->quit = true;
        return;
      } else if (w == 0x00E0) {
        // 00E0: CLEAR
        chip8_clear(ch8);
        chip8_advance(ch8);
      } else if (w == 0x00EE) {
        // 00EE: RETURN
        chip8_return(ch8);

      } else {
        chip8_error(ch8, "Invalid instruction");
        return;
      }

      break;
    }

    case 0x1: {
      // 1NNN: JUMP
      chip8_jump(ch8, destination);
      break;
    }

    case 0x2: {
      // 2NNN: SUBROUTINE
      chip8_subroutine(ch8, destination);
      break;
    }

    case 0x3: {
      // 3XNN: IFNEQ (SKIP NEXT IF RX = NN)
      if (ch8->R[x] == low) {
        chip8_advance(ch8);
      }
      chip8_advance(ch8);
      break;
    }

    case 0x4: {
      // 4XNN: IFEQ (SKIP NEXT IF RX != NN)
      if (ch8->R[x] != low) {
        chip8_advance(ch8);
      }
      chip8_advance(ch8);
      break;
    }

    case 0x5: {

      if (lastnibble == 0x0) {
        // 5XY0: IFNEQ (SKIP NEXT IF RX = RY)
        if (ch8->R[x] == ch8->R[y]) {
          chip8_advance(ch8);
        }
        chip8_advance(ch8);
      } else if (lastnibble == 0x1) {
        // 5XY1 NON-STANDARD: PIXEL
        chip8_pixel(ch8, ch8->R[x], ch8->R[y]);
        chip8_advance(ch8);
      } else {
        chip8_error(ch8, "Invalid instruction");
        return;
      }
      break;
    }

    case 0x6: {
      // 6XNN: SET RX, NN
      ch8->R[x] = low;
      chip8_advance(ch8);
      break;
    }

    case 0x7: {
      // 7XNN: ADD RX, NN
      ch8->R[x] += low; // no carry flag change!
      chip8_advance(ch8);
      break;
    }

    case 0x8: {
      switch (lastnibble) {
        case 0x0: {
          // 8XY0: SET RX, RY
          ch8->R[x] = ch8->R[y];
          chip8_advance(ch8);
          break;
        }

        case 0x1: {
          // 8XY1: OR RX, RY
          ch8->R[x] |= ch8->R[y];
          chip8_advance(ch8);
          break;
        }

        case 0x2: {
          // 8XY2: AND RX, RY
          ch8->R[x] &= ch8->R[y];
          chip8_advance(ch8);
          break;
        }

        case 0x3: {
          // 8XY3: XOR RX, RY
          ch8->R[x] ^= ch8->R[y];
          chip8_advance(ch8);
          break;
        }

        case 0x4: {
          // 8XY4: ADD RX, RY
          // set RF if overflow
          if (ch8->R[x] + ch8->R[y] > 255)
            ch8->R[0xF] = 1;
          else
            ch8->R[0xF] = 0;
            
          ch8->R[x] += ch8->R[y];
          chip8_advance(ch8);
          break;
        }

        case 0x5: {
          // 8XY5: SUB RX, RY
          // set RF if no borrow
          if (ch8->R[x] >= ch8->R[y])
            ch8->R[0xF] = 1;
          else
            ch8->R[0xF] = 0;

          ch8->R[x] -= ch8->R[y];
          chip8_advance(ch8);
          break;
        }

        case 0x6: {
          // 8XY6: RSHIFT RX
          // set RF to RX's LSB
          ch8->R[0xF] = ch8->R[x] & 0x01;
          ch8->R[x] >>= 1;
          chip8_advance(ch8);
          break;
        }

        case 0x7: {
          // 8XY7: REVSUB RX, RY
          if (ch8->R[x] > ch8->R[y])
            ch8->R[0xF] = 1;
          else
            ch8->R[0xF] = 0;

          ch8->R[x] = ch8->R[y] - ch8->R[x];
          chip8_advance(ch8);
          break;
        }

        case 0xE: {
          // 8XYE: LSHIFT RX
          ch8->R[0xF] = (ch8->R[x] & 128) >> 7;
          ch8->R[x] <<= 1;
          chip8_advance(ch8);
          break;
        }

        default: {
          chip8_error(ch8, "Invalid instruction");
          return;
        }
      }
      break;
    }

    case 0x9: {
      if (lastnibble == 0x0) {
        // 9XY0: IFEQ RX, RY (SKIP NEXT IF RX != RY)
        if (ch8->R[x] != ch8->R[y]) {
          chip8_advance(ch8);
        }
        chip8_advance(ch8);
      } else {
        chip8_error(ch8, "Invalid instruction");
        return;
      }
      break;
    }

    case 0xA: {
      // ANNN: SET I, NNN
      ch8->I = destination;
      chip8_advance(ch8);
      break;
    }

    case 0xB: {
      // BNNN: JUMP NNN, R0 (NNN + R0)
      chip8_jump(ch8, destination + ch8->R[0]);
      break;
    }

    case 0xC: {
      // CXNN: RANDOM RX, NN (RX = RANDOM BYTE & NN)
      ch8->R[x] = (rand() % 255) & low;
      chip8_advance(ch8);
      break;
    }

    case 0xD: {
      // DXYN: DRAW RX, RY, N
      chip8_draw(ch8, ch8->R[x], ch8->R[y], lastnibble);
      chip8_advance(ch8);
      break;
    }

    case 0xE: {
      if (low == 0x9E) {
        // EX9E: IFNKEY RX (SKIP NEXT IF KEY IN RX IS PRESSED)
        byte key = ch8->R[x] & 0x0F;
        if (ch8->keys[key]) {
          chip8_advance(ch8);
        }
        chip8_advance(ch8);
      } else if (low == 0xA1) {
        // EX9E: IFKEY RX (SKIP NEXT IF KEY IN RX IS NOT PRESSED)
        byte key = ch8->R[x] & 0x0F;
        if (!ch8->keys[key]) {
          chip8_advance(ch8);
        }
        chip8_advance(ch8);
      } else {
        chip8_error(ch8, "Invalid instruction");
        return;
      }
      break;
    }

    case 0xF: {
      switch (low) {
        case 0x07: {
          // FX07: SET RX, TIMER
          ch8->R[x] = ch8->timer;
          chip8_advance(ch8);
          break;
        }

        case 0x0A: {
          // FX0A: AWAIT KEYPRESS
          //todo
          chip8_advance(ch8);
          break;
        }

        case 0x15: {
          // FX15: SET TIMER, RX
          ch8->timer = ch8->R[x];
          chip8_advance(ch8);
          break;
        }

        case 0x18: {
          // FX18: SET SOUND, RX
          ch8->sound = ch8->R[x];
          chip8_advance(ch8);
          break;
        }

        case 0x1E: {
          // FX1E: ADD I, RX
          ch8->I += ch8->R[x];
          chip8_advance(ch8);
          break;
        }

        case 0x29: {
          // FX29: SETSPRITE RX
          ch8->I = (ch8->R[x] & 0x0F) * 5;
          chip8_advance(ch8);
          break;
        }

        case 0x33: {
          // FX33: BCD RX
          //todo
          chip8_advance(ch8);
          break;
        }

        case 0x55: {
          // FX55: DUMP RX
          for (int i=0; i<=x; i++) {
            ch8->mem[i+ch8->I] = ch8->R[i];
          }
          chip8_advance(ch8);
          break;
        }

        case 0x65: {
          // FX65: FILL RX
          for (int i=0; i<=x; i++) {
            ch8->mem[i+ch8->I] = ch8->R[i];
          }
          chip8_advance(ch8);
          break;
        }

        default: {
          chip8_error(ch8, "Invalid instruction");
          return;
        }
      }
      break;
    }


  }


}
