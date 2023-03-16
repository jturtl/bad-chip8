#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "compile.h"

char *ReadFile(const char *path) {
  char *buffer = 0;
  unsigned long length;
  FILE *f = fopen(path, "rb");

  if (f)
  {
    fseek (f, 0, SEEK_END);
    length = ftell (f);
    fseek (f, 0, SEEK_SET);
    buffer = malloc (length+1);
    if (buffer)
    {
      fread (buffer, 1, length, f);
    }
    fclose (f);
    buffer[length] = '\0';
    return buffer;
  } else {
    return NULL;
  }
}

int main(int argc, char *argv[]) {

  puts("CHIP-8 ASSEMBLER");

  if (argc < 2) {
    puts("Usage: ch8asm <source file> [-d]");
    return 0;
  }

  // dump mode: print information through every step of the process
  bool dump = false;
  if (argc > 2) {
    if (strcmp(argv[2], "-d") == 0) {
      dump = true;
    }
  }

  printf("\nASSEMBLING '%s'\n", argv[1]);

  char *src = ReadFile(argv[1]);
  if (src == NULL) {
    printf("Failed to read file '%s'\n", argv[1]);
    return 1;
  }

  if (dump) {
    printf("\n===== FILE CONTENT =====\n");

    printf("%s", src);

    printf("\n===== END FILE CONTENT =====\n");
  }

  if (dump) printf("\nScanning...\n");
  bool wasError;
  TokenList tl = Tokenize(src, &wasError);
  if (dump) printf("End scan.\n");

  free(src);

  if (wasError) {

    printf("\n[!] ERRORS OCCURRED DURING SCAN.\n");
    free(tl.data);

    return 1;

  }

  if (dump) {
    printf("\n===== LISTING TOKENS =====\n");
    for (unsigned long i=0; i<tl.len; i++) {
      Token tk = tl.data[i];
      printf(
        "%16s  |  type = %2d  |  num = %5d  |  line = %ld\n",
        tk.strData, tk.type,  tk.numData,   tk.line
      );
    }
    printf("===== END TOKENS =====\n");
  }

  if (dump) printf("\nCompiling...\n");

  CompiledCode result = Compile(tl, &wasError);
  free(tl.data);

  if (dump) printf("End compile.\n");

  if (wasError) {
    printf("\n[!] ERRORS OCCURRED DURING COMPILATION.\n");
    free(result.data);
    return 1;
  }

  if (dump) {
    printf("\n===== HEX DUMP =====\n");

    int column = 0;
    bool toggle = true;
    for (int i=0; i<result.length; i++) {
      if (column == 32) {
        printf("\n");
        column = 0;
      }
      if (column == 0) {
        printf("%03X - %03X |  ",
          i+CHIP8_START_ADDRESS,
          i + 31 >= result.length ?
            result.length-1 + CHIP8_START_ADDRESS
            :
            i + 31 + CHIP8_START_ADDRESS
        );
      }

      if (toggle) {
        printf("%02X", result.data[i]);
      } else {
        printf("%02X ", result.data[i]);
      }

      toggle = !toggle;
      column++;
    }

    printf("\n===== END HEX DUMP =====\n");
  }

  if (dump) printf("\nWriting...\n");
  FILE *fp = fopen("out.ch8rom", "wb");
  if (fp == NULL) {
    printf("[!] COULD NOT OPEN OUTPUT FILE 'out.ch8rom'\n");
    free(result.data);
    return 1;
  }

  printf("Writing to 'out.ch8rom'\n");

  fwrite(result.data, 1, result.length, fp);

  fclose(fp);

  if (dump) printf("End write.\n");

  free(result.data);

  return 0;
}
