//
// This program is designed to add an identification block to the end of a bootloader.
// The block contains information about the chipset code and the loading address.
//
//
#include <stdio.h>

void main(int argc, char* argv[]) {

FILE* ldr;
unsigned int code, adr;
unsigned int i;

  
if ((argc != 2) && (argc != 4)) {
  printf("\n Command line format:\n\
  %s  <bootloader file> <chipset code> <loading address> - write an identification block to the bootloader\n\
  %s  <bootloader file> - view the identification block\n", argv[0], argv[0]);
  return;
}

ldr = fopen(argv[1], "r+");
if (ldr == 0) {
  printf("\n Error opening the file %s\n", argv[1]);
  return;
}

//------------ View identification block mode ---------------
if (argc == 2) {
  fseek(ldr, -12, SEEK_END);
  fread(&i, 1, 4, ldr);
  if (i != 0xdeadbeef) {
     printf("\n The bootloader does not contain an identification block\n");
     fclose(ldr);
     return;
   }
  fread(&code, 4, 1, ldr);
  fread(&adr, 4, 1, ldr);
  fclose(ldr);
  printf("\n Bootloader identification parameters for %s:", argv[1]);
  printf("\n * Chipset code = 0x%x", code);
  printf("\n * Loading address = 0x%08x\n\n", adr);
  return;
}

//----------- Write identification block mode ---------------

sscanf(argv[2], "%x", &code);
if (code == 0) {
  printf("\n Incorrect chipset code\n");
  fclose(ldr);
  return;
}

sscanf(argv[3], "%x", &adr);
if (adr == 0) {
  printf("\n Incorrect address\n");
  fclose(ldr);
  return;
}

fseek(ldr, -12, SEEK_END);
fread(&i, 1, 4, ldr);
if (i == 0xdeadbeef) {
  printf("\n The bootloader already contains an identification block - overwriting\n");
}
else {
  fclose(ldr);
  ldr = fopen(argv[1], "a");
  i = 0xdeadbeef;
  fwrite(&i, 4, 1, ldr);
}  
fwrite(&code, 4, 1, ldr);
fwrite(&adr, 4, 1, ldr);
fclose(ldr);
}
