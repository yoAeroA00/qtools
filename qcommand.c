#include "include.h"

// Flag for sending the 7E prefix
int prefixFlag = 1;

// HDLC mode flag
int hdlcFlag = 1;

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
// Interactive shell for entering commands into the bootloader
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

//*******************************************************************
// Send the command buffer to the modem and unfreeze the result
//*******************************************************************
void iocmd(char* cmdBuffer, int cmdLength) {
    unsigned char ioBuffer[2048];
    unsigned int ioLength;

    if (hdlcFlag) {
        // HDLC mode command
        ioLength = send_cmd_base(cmdBuffer, cmdLength, ioBuffer, prefixFlag);
        if (ioBuffer[1] == 0x0E) {
            show_errpacket("[ERR] ", ioBuffer, ioLength);
            return;
        }
    } else {
        qwrite(siofd, cmdBuffer, cmdLength);
        ioLength = qread(siofd, ioBuffer, 1024);
    }

    if (ioLength != 0) {
        printf("\n ---- response --- \n");
        dump(ioBuffer, ioLength, 0);
    }
    printf("\n");
}

//*******************************************************************
//* Search for non-space characters in a string
//*
//* zmode - 
//*  0 - search for the first non-space character
//*  1 - search for a space first, then a non-space character
//*******************************************************************
char* findToken(char* line, int zeroMode) {
    int i = 0;

    if (zeroMode) {
        // Look for a space
        for (i = 0; line[i] != ' '; i++) {
            if (line[i] == '\r' || line[i] == '\n' || line[i] == 0) {
                return 0; // Logical end of the string
            }
        }
    }

    for (; line[i] != 0; i++) {
        if (line[i] == '\r' || line[i] == '\n') {
            return 0; // Logical end of the string
        }
        if (line[i] != ' ') {
            return line + i;
        }
    }
    return 0;
}

//*******************************************************************
// Parse the entered command sequence and execute it
//*******************************************************************
void asciiCmd(char* line) {
    char* sptr;
    unsigned char cmdBuffer[2048];
    int byteCount = 0;
    int i;

    sptr = findToken(line, 0); // Parse command bytes separated by spaces
    if (sptr == 0) {
        return; // Empty command string
    }

    do {
        if (*sptr == '\"') {
            // Enter ASCII text mode
            sptr++;
            while (*sptr != '\"') { // up to the quotation mark
                if (*sptr == 0 || *sptr == '\n' || *sptr == '\r') {
                    printf("\n Closing quote is missing\n");
                    return;
                }
                cmdBuffer[byteCount++] = *sptr++;
            }
        } else {
            // Enter hex byte mode
            sscanf(sptr, "%x", &i);
            cmdBuffer[byteCount++] = i;
        }
    } while ((sptr = findToken(sptr, 1)) != 0);

    //printf("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    //dump(cmdbuf,bcnt,0);
    iocmd(cmdBuffer, byteCount);
}

//*******************************************************************
// Send the content of a file as a command to the modem
//*******************************************************************
void binaryCmd(char* line) {
    unsigned char cmdBuffer[2048];
    FILE* commandFile;
    unsigned int i;

    char* sptr;
    sptr = strtok(line, " "); // Extract the file name
    if (sptr == 0) {
        printf(" File name not specified\n");
        return;
    }
    commandFile = fopen(sptr, "r");
    if (commandFile == 0) {
        printf(" Error opening the file %s\n", sptr);
        return;
    }
    fseek(commandFile, 0, SEEK_END);
    i = ftell(commandFile);
    if (i > 1024) {
        printf(" The file is too large - %u bytes\n", i);
        fclose(commandFile);
        return;
    }
    rewind(commandFile);
    fread(cmdBuffer, i, 1, commandFile);
    fclose(commandFile);
    iocmd(cmdBuffer, i);
}

//*******************************************************************
// Switch HDLC mode on or off
//*******************************************************************
void hdlcSwitch(char* line) {
    char* sptr;
    unsigned int mode;
    sptr = strtok(line, " "); // Extract the parameter

    if (sptr != 0) {
        // The mode is specified - set it
        sscanf(sptr, "%u", &mode);
        hdlcFlag = mode ? 1 : 0;
    }
    printf(" HDLC %s\n", hdlcFlag ? "On" : "Off");
}

//**********************************************
// Decode the contents of CFG0 register
//**********************************************
void decodeCfg0() {
    unsigned int cfg0 = mempeek(nand_cfg0);
    printf("\n **** Configuration Register 0 *****");
    printf("\n * NUM_ADDR_CYCLES              = %x", (cfg0 >> 27) & 7);
    printf("\n * SPARE_SIZE_BYTES             = %x", (cfg0 >> 23) & 0xf);
    printf("\n * ECC_PARITY_SIZE_BYTES        = %x", (cfg0 >> 19) & 0xf);
    printf("\n * UD_SIZE_BYTES                = %x", (cfg0 >> 9) & 0x3ff);
    printf("\n * CW_PER_PAGE                  = %x", ((cfg0 >> 6) & 7) | ((cfg0 >> 2) & 8));
    printf("\n * DISABLE_STATUS_AFTER_WRITE   = %x", (cfg0 >> 4) & 1);
    printf("\n * BUSY_TIMEOUT_ERROR_SELECT    = %x", (cfg0) & 7);
}

//**********************************************
// Decode the contents of CFG1 register
//**********************************************
void decodeCfg1() {
    unsigned int cfg1 = mempeek(nand_cfg1);
    printf("\n **** Configuration Register 1 *****");
    printf("\n * ECC_MODE                      = %x", (cfg1 >> 28) & 3);
    printf("\n * ENABLE_BCH_ECC                = %x", (cfg1 >> 27) & 1);
    printf("\n * DISABLE_ECC_RESET_AFTER_OPDONE= %x", (cfg1 >> 25) & 1);
    printf("\n * ECC_DECODER_CGC_EN            = %x", (cfg1 >> 24) & 1);
    printf("\n * ECC_ENCODER_CGC_EN            = %x", (cfg1 >> 23) & 1);
    printf("\n * WR_RD_BSY_GAP                 = %x", (cfg1 >> 17) & 0x3f);
    printf("\n * BAD_BLOCK_IN_SPARE_AREA       = %x", (cfg1 >> 16) & 1);
    printf("\n * BAD_BLOCK_BYTE_NUM            = %x", (cfg1 >> 6) & 0x3ff);
    printf("\n * CS_ACTIVE_BSY                 = %x", (cfg1 >> 5) & 1);
    printf("\n * NAND_RECOVERY_CYCLES          = %x", (cfg1 >> 2) & 7);
    printf("\n * WIDE_FLASH                    = %x", (cfg1 >> 1) & 1);
    printf("\n * ECC_DISABLE                   = %x", (cfg1) & 1);
}

//**********************************************8
// Command processing
//**********************************************8
void processCommand(char* cmdLine) {
    int adr, len = 128, data;
    char* sptr;
    char membuf[4096];
    int block, page, sect;

switch (cmdLine[0]) {

  // Help
  case 'h':
    printf("\n Available commands:\n\n\
c nn nn nn nn.... - generate and execute a command packet from the listed bytes\n\
@ file            - execute a command packet from the specified file\n\
d adr [len]       - view the dump of the system's address space\n\
m adr word ...    - write words to the specified address\n\
r block page sect - read a flash block into a sector buffer \n\
s                 - view the dump of the NAND controller sector buffer\n\
n                 - view the contents of NAND controller registers\n\
k                 - decode the contents of configuration registers\n\
i [s]             - start the HELLO procedure, s - without configuring the settings\n\
f [n]             - enable(1)/disable(0)/view the state of HDLC mode\n\
x                 - exit the program\n\
\n");
    break;
  
  // Processing a command packet
  case 'c':
    asciiCmd(cmdLine + 1);
    break;
 
  case '@':
    binaryCmd(cmdLine + 1);
    break;

  case 'f':
    hdlcSwitch(cmdLine + 1);
    break;
    
  // Activating the bootloader
  case 'i':
    sptr = strtok(cmdLine + 1, " "); // address
    if ((sptr == 0) || (sptr[0] == 0x0a)) {
      hello(1);
      break;
    }
    if (sptr[0] != 's') {
      printf("\n Invalid parameter in the i command");
      break;
    }
    hello(2);
    break;
    
  // Memory dump
  case 'd':  
    sptr = strtok(cmdLine + 1, " "); // address
    if (sptr == 0) {
      printf("\n Address not specified");
      return;
    }
    sscanf(sptr, "%x", &adr);
    sptr = strtok(0, " "); // length
    if (sptr != 0) {
      sscanf(sptr, "%x", &len);
    }
    if (memread(membuf, adr, len)) {
      dump(membuf, len, adr); 
    }
    break;

  case 'm':
    sptr = strtok(cmdLine + 1, " "); // address
    if (sptr == 0) {
      printf("\n Address not specified");
      return;
    }
    sscanf(sptr, "%x", &adr);
    while ((sptr = strtok(0, " ")) != 0) { // data
      sscanf(sptr, "%x", &data);
      if (!mempoke(adr, data)) {
        printf("\n Command returned an error, adr=%08x  data=%08x\n", adr, data);
      }
      adr += 4;
    }
    break;

  case 'r':
    hello(0);
    sptr = strtok(cmdLine + 1, " "); // block
    if (sptr == 0) {
      printf("\n Block # not specified");
      return;
    }
    sscanf(sptr, "%x", &block);

    sptr = strtok(0, " "); // page
    if (sptr == 0) {
      printf("\n Page # not specified");
      return;
    }
    sscanf(sptr, "%x", &page);
    if (page > 63)  {
      printf("\n Page # is too large");
      return;
    }
   
    sptr = strtok(0, " "); // sector
    if (sptr == 0) {
      printf("\n Sector # not specified");
      return;
    }
    sscanf(sptr, "%x", &sect);
    if (sect > spp - 1)  {
      printf("\n Sector # is too large");
      return;
    }
   
    if (!flash_read(block, page, sect)) {
      printf("\n    *** bad block ***\n");
    }
    memread(membuf, sector_buf, 0x23c);
    dump(membuf, 0x23c, 0); 
    break;
   
  case 's': 
    hello(0);
    memread(membuf, sector_buf, 0x23c);
    dump(membuf, 0x23c, 0); 
    break;

  case 'n':
    hello(0);
    if (is_chipset("MDM9x4x")) {
      memread(membuf, nand_cmd, 0x1c);
      memread(membuf + 0x20, nand_cmd + 0x20, 0x0c);
      memread(membuf + 0x40, nand_cmd + 0x40, 0x0c);
      //   memread(membuf+0x64,nand_cmd+0x64,4);
      //   memread(membuf+0x70,nand_cmd+0x70,0x1c);
      //   memread(membuf+0xa0,nand_cmd+0xa0,0x10);
      //   memread(membuf+0xd0,nand_cmd+0xd0,0x10);
      memread(membuf+0xe8,nand_cmd+0xe8,0x0c);
    } else {
      memread(membuf, nand_cmd, 0x100);
    }

    printf("\n* 000 NAND_FLASH_CMD         = %08x", *((unsigned int*)&membuf[0]));
    printf("\n* 004 NAND_ADDR0             = %08x", *((unsigned int*)&membuf[4]));
    printf("\n* 008 NAND_ADDR1             = %08x", *((unsigned int*)&membuf[8]));
    printf("\n* 00c NAND_CHIP_SELECT       = %08x", *((unsigned int*)&membuf[0xc]));
    printf("\n* 010 NANDC_EXEC_CMD         = %08x", *((unsigned int*)&membuf[0x10]));
    printf("\n* 014 NAND_FLASH_STATUS      = %08x", *((unsigned int*)&membuf[0x14]));
    printf("\n* 018 NANDC_BUFFER_STATUS    = %08x", *((unsigned int*)&membuf[0x18]));
    printf("\n* 020 NAND_DEV0_CFG0         = %08x", *((unsigned int*)&membuf[0x20]));
    printf("\n* 024 NAND_DEV0_CFG1         = %08x", *((unsigned int*)&membuf[0x24]));
    printf("\n* 028 NAND_DEV0_ECC_CFG      = %08x", *((unsigned int*)&membuf[0x28]));
    printf("\n* 040 NAND_FLASH_READ_ID     = %08x", *((unsigned int*)&membuf[0x40]));
    printf("\n* 044 NAND_FLASH_READ_STATUS = %08x", *((unsigned int*)&membuf[0x44]));
    printf("\n* 048 NAND_FLASH_READ_ID2    = %08x", *((unsigned int*)&membuf[0x48]));
    if (!(is_chipset("MDM9x4x"))) {
      printf("\n* 064 FLASH_MACRO1_REG       = %08x", *((unsigned int*)&membuf[0x64]));
      printf("\n* 070 FLASH_XFR_STEP1        = %08x", *((unsigned int*)&membuf[0x70]));
      printf("\n* 074 FLASH_XFR_STEP2        = %08x", *((unsigned int*)&membuf[0x74]));
      printf("\n* 078 FLASH_XFR_STEP3        = %08x", *((unsigned int*)&membuf[0x78]));
      printf("\n* 07c FLASH_XFR_STEP4        = %08x", *((unsigned int*)&membuf[0x7c]));
      printf("\n* 080 FLASH_XFR_STEP5        = %08x", *((unsigned int*)&membuf[0x80]));
      printf("\n* 084 FLASH_XFR_STEP6        = %08x", *((unsigned int*)&membuf[0x84]));
      printf("\n* 088 FLASH_XFR_STEP7        = %08x", *((unsigned int*)&membuf[0x88]));
      printf("\n* 0a0 FLASH_DEV_CMD0         = %08x", *((unsigned int*)&membuf[0xa0]));
      printf("\n* 0a4 FLASH_DEV_CMD1         = %08x", *((unsigned int*)&membuf[0xa4]));
      printf("\n* 0a8 FLASH_DEV_CMD2         = %08x", *((unsigned int*)&membuf[0xa8]));
      printf("\n* 0ac FLASH_DEV_CMD_VLD      = %08x", *((unsigned int*)&membuf[0xac]));
      printf("\n* 0d0 FLASH_DEV_CMD3         = %08x", *((unsigned int*)&membuf[0xd0]));
      printf("\n* 0d4 FLASH_DEV_CMD4         = %08x", *((unsigned int*)&membuf[0xd4]));
      printf("\n* 0d8 FLASH_DEV_CMD5         = %08x", *((unsigned int*)&membuf[0xd8]));
      printf("\n* 0dc FLASH_DEV_CMD6         = %08x", *((unsigned int*)&membuf[0xdc]));
    }
    printf("\n* 0e8 NAND_ERASED_CW_DET_CFG = %08x", *((unsigned int*)&membuf[0xe8]));
    printf("\n* 0ec NAND_ERASED_CW_DET_ST  = %08x", *((unsigned int*)&membuf[0xec]));
    printf("\n* 0f0 EBI2_ECC_BUF_CFG       = %08x\n", *((unsigned int*)&membuf[0xf0]));
    break;
  case 'x': 
    exit(0);
    break;

  case 'k':
    hello(0);
    decodeCfg0();
    printf("\n");
    decodeCfg1();
    printf("\n");
    break;
   
  default:
    printf("\nUndefined command\n");
    break;
}   
}    

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

// Interactive shell's main function
void main(int argc, char* argv[]) {
  
#ifndef WIN32
  char* line;
  char oldCmdLine[1024] = "";
#else
  char line[1024];
#endif
  char sCmdLine[1024] = {0};
#ifndef WIN32
  char devName[50] = "/dev/ttyUSB0";
#else
  char devName[50] = "";
#endif
  int opt, helloFlag = 0;

  while ((opt = getopt(argc, argv, "p:ichekf")) != -1) {
    switch (opt) {
      case 'h': 
        printf("\nInteractive shell for entering commands into the bootloader\n\n\
Available options:\n\n\
-p <tty>       - specifies the name of the serial port device to communicate with the bootloader\n\
-i             - starts the HELLO procedure to initialize the bootloader\n\
-e             - disables sending the 7E prefix before the command\n\
-f             - disables HDLC formatting of command packets\n\
-k #           - chipset code (-kl - get a list of codes)\n\
-c \"<command>\" - runs the specified command and exits\n");
        return;
         
      case 'p':
        strcpy(devName, optarg);
        break;

      case 'f':
        hdlcFlag = 0;
        break;
      
      case 'k':
        define_chipset(optarg);
        break;
      
      case 'i':
        helloFlag = 1;
        break;
        
      case 'e':
        prefixFlag = 0;
        break;
         
      case 'c':
        strcpy(sCmdLine, optarg);
        break;

      case '?':
      case ':':  
        return;
    }
  }  

#ifdef WIN32
  if (*devName == '\0') {
    printf("\n - Serial port not specified\n"); 
    return; 
  }
#endif

  if (!open_port(devName))  {
#ifndef WIN32
    printf("\n - Serial port %s does not open\n", devName); 
#else
    printf("\n - Serial port COM%s does not open\n", devName); 
#endif
    return; 
  }
  if (helloFlag) {
    hello(1);
  }
  printf("\n");

  // Run the command specified by the -c option if available
  if (strlen(sCmdLine) != 0) {
    processCommand(sCmdLine);
    return;
  }
 
// Main Command Processing Loop
#ifndef WIN32
  // Load command history
  readHistory("qcommand.history");
  writeHistory("qcommand.history");
#endif 

  for (;;) {
#ifndef WIN32
    line = readLine(">");
#else
    printf(">");
    fgets(line, sizeof(line), stdin);
#endif
    if (line == 0) {
      printf("\n");
      return;
    }   
    if (strlen(line) < 1) {
      continue; // Too short command
    }
#ifndef WIN32
    if (strcmp(line, oldCmdLine) != 0) {
      addHistory(line); // Add to the history buffer
      appendHistory(1, "qcommand.history");
      strcpy(oldCmdLine, line);
    }  
#endif
    processCommand(line);
#ifndef WIN32
    free(line);
#endif
  } 
}
