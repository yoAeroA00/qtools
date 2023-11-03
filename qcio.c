//
//  Drivers for interacting with a Flash modem through NAND controller access and bootloader procedures
//
#include "include.h"

// Global variables - we collect them here

unsigned int nand_cmd = 0x1b400000;
unsigned int spp = 0;
unsigned int pagesize = 0;
unsigned int sectorsize = 512;
unsigned int maxblock = 0;     // Total number of flash blocks
char flash_mfr[30] = {0};
char flash_descr[30] = {0};
unsigned int oobsize = 0;
unsigned int bad_loader = 0;
unsigned int flash16bit = 0; // 0 - 8-bit flash, 1 - 16-bit flash

unsigned int badsector;    // sector containing a defective block
unsigned int badflag;      // marker for a defective block
unsigned int badposition;  // position of the defective block marker
unsigned int badplace;     // location of the marker: 0-user, 1-spare
int bch_mode = 0;            // ECC mode: 0=R-S  1=BCH
int ecc_size;              // ECC size
int ecc_bit;               // Number of bits corrected by ECC

//****************************************************************
//* Wait for the completion of an operation performed by the NAND controller
//****************************************************************
void nandwait() { 
   if (get_controller() == 0) 
     while ((mempeek(nand_status) & 0xf) != 0);  // MDM
   else
     while ((mempeek(nand_status) & 0x3) != 0);  // MSM
}

//*************************************88
//* Set address registers 
//*************************************
void setaddr(int block, int page) {

int adr;  
  
adr = block * ppb + page;  // Page number from the beginning of the flash

if (get_controller() == 0) {
  // MDM
  mempoke(nand_addr0, adr << 16);         // Lower part of the address. 16 bits column address is set to 0
  mempoke(nand_addr1, (adr >> 16) & 0xff);  // Only the high byte of the address
}  
else {
  // MSM
  mempoke(nand_addr0, adr << 8);
}
}

//***************************************************************
//* Start executing a NAND controller command with waiting
//***************************************************************
void exec_nand(int cmd) {

if (get_controller() == 0) {
  // MDM  
  mempoke(nand_cmd, cmd); // Reset all controller operations
  mempoke(nand_exec, 0x1);
  nandwait();
}
else {
  // MSM
  mempoke(nand_cmd, cmd); // Reset all controller operations
  nandwait();
}
}

//*********************************************
//* Reset NAND controller
//*********************************************
void nand_reset() {

exec_nand(1);  
}

//*********************************************
//* Read a sector of the flash at the specified address 
//*********************************************

int flash_read(int block, int page, int sect) {
  
int i;

nand_reset();
// Address
setaddr(block, page);
if (get_controller() == 0) {
  // MDM - set the command code once
  mempoke(nand_cmd, 0x34); // read data+ecc+spare
  // Read loop to the required sector
  for(i = 0; i < (sect + 1); i++) {
    mempoke(nand_exec, 0x1);
    nandwait();
  }  
}
else {
  // MSM - the command code is entered into the command register every time
  for(i = 0; i < (sect + 1); i++) {
    mempoke(nand_cmd, 0x34); // read data+ecc+spare
    nandwait();
  }  
}  
if (test_badblock()) return 0;
return 1;  
}

//**********************************************8
//* Procedure to activate the hello loader
//*
//* mode=0 - hello necessity auto-detection
//* mode=1 - force hello execution
//* mode=2 - force hello execution without configuring the chipset 
//**********************************************8
void hello(int mode) {

int i;  
unsigned char rbuf[1024];
char hellocmd[] = "\x01QCOM fast download protocol host\x03### ";

// Applet for checking the bootloader's operability
unsigned char cmdbuf[] = {
  0x11, 0x00, 0x12, 0x00, 0xa0, 0xe3, 0x00, 0x00,
  0xc1, 0xe5, 0x01, 0x40, 0xa0, 0xe3, 0x1e, 0xff,
  0x2f, 0xe1
};
unsigned int cfg0;

// Silent initialization mode
if (mode == 0) {
  i = send_cmd(cmdbuf, sizeof(cmdbuf), rbuf);
  ttyflush();
  i = rbuf[1];
  // Check if the bootloader has been initialized previously
  if (i == 0x12) {
     if (!test_loader()) {
       printf("\n An unpatched bootloader is being used - continuing is not possible\n");
       exit(1);
     }
	 //printf("\n chipset = %i  base = %i",chip_type,name);
     get_flash_config();
     return;
  }  
  qread(siofd, rbuf, 1024);   // Clear the buffer tail with an error message
}  

i = send_cmd(hellocmd, strlen(hellocmd), rbuf);
if (rbuf[1] != 2) {
   printf(" Sending hello...");
   i = send_cmd(hellocmd, strlen(hellocmd), rbuf);
   if (rbuf[1] != 2) {
     printf(" returned an error on re-sending hello!\n");
     dump(rbuf, i, 0);
     return;
   }  
   printf("ok");
}  
i = rbuf[0x2c];
rbuf[0x2d + i] = 0;
if (mode == 2) {
   // Silent execution - bypass chipset configuration
   printf("Hello ok, flash memory: %s\n", rbuf + 0x2d);
   return; 
 }  
ttyflush(); 
if (!test_loader()) {
  printf("\n An unpatched bootloader is being used - continuing is not possible\n");
  exit(1);
}  

if (get_sahara()) disable_bam(); // Disable NANDc BAM if working with new-generation chipsets

get_flash_config();
cfg0 = mempeek(nand_cfg0);
printf("\n HELLO Protocol Version: %i", rbuf[0x22]); 
printf("\n Chipset: %s", get_chipname()); 
printf("\n NAND Controller Base Address: %08x", nand_cmd);
printf("\n Flash Memory: %s %s, %s", flash_mfr, (rbuf[0x2d] != 0x65) ? ((char*)(rbuf + 0x2d)) : "", flash_descr);
//printf("\n Maximum Packet Size: %i bytes", *((unsigned int*)&rbuf[0x24]));
printf("\n Sector Size: %u bytes", (cfg0 & (0x3ff << 9)) >> 9);
printf("\n Page Size: %u bytes (%u sectors)", pagesize, spp);
printf("\n Number of Pages per Block: %u", ppb);
printf("\n OOB Size: %u bytes"); 
printf("\n ECC Type: %s, %i bits", bch_mode ? "BCH" : "R-S", ecc_bit);
printf("\n ECC Size: %u bytes", ecc_size);
printf("\n Spare Size: %u bytes", (cfg0 >> 23) & 0xf);
printf("\n Defective Block Marker Position: ");
printf("%s+%x", badplace ? "spare" : "user", badposition);
printf("\n Total Flash Memory Size = %u blocks (%i MB)", maxblock, maxblock * ppb / 1024 * pagesize / 1024);
printf("\n");
}

//**********************************************************
//* Retrieving NAND Flash Memory Format Parameters from the Controller
//**********************************************************
void get_flash_config() {
  
unsigned int cfg0, cfg1, nandid, pid, fid, blocksize, devcfg, chipsize;
unsigned int ecccfg;
int linuxcwsize;
int i;
int c_badmark_pos; // Calculated bad block marker position

struct {
  char* type;   // Textual description of the type
  unsigned int id;      // Flash memory ID
  unsigned int chipsize; // Flash memory size in megabytes
} nand_ids[]= {

	{"NAND 16MiB 1,8V 8-bit",	0x33, 16},
	{"NAND 16MiB 3,3V 8-bit",	0x73, 16}, 
	{"NAND 16MiB 1,8V 16-bit",	0x43, 16}, 
	{"NAND 16MiB 3,3V 16-bit",	0x53, 16}, 

	{"NAND 32MiB 1,8V 8-bit",	0x35, 32},
	{"NAND 32MiB 3,3V 8-bit",	0x75, 32},
	{"NAND 32MiB 1,8V 16-bit",	0x45, 32},
	{"NAND 32MiB 3,3V 16-bit",	0x55, 32},

	{"NAND 64MiB 1,8V 8-bit",	0x36, 64},
	{"NAND 64MiB 3,3V 8-bit",	0x76, 64},
	{"NAND 64MiB 1,8V 16-bit",	0x46, 64},
	{"NAND 64MiB 3,3V 16-bit",	0x56, 64},

	{"NAND 128MiB 1,8V 8-bit",	0x78, 128},
	{"NAND 128MiB 1,8V 8-bit",	0x39, 128},
	{"NAND 128MiB 3,3V 8-bit",	0x79, 128},
	{"NAND 128MiB 1,8V 16-bit",	0x72, 128},
	{"NAND 128MiB 1,8V 16-bit",	0x49, 128},
	{"NAND 128MiB 3,3V 16-bit",	0x74, 128},
	{"NAND 128MiB 3,3V 16-bit",	0x59, 128},

	{"NAND 256MiB 3,3V 8-bit",	0x71, 256},

	/*512 Megabit */
	{"NAND 64MiB 1,8V 8-bit",	0xA2, 64},   
	{"NAND 64MiB 1,8V 8-bit",	0xA0, 64},
	{"NAND 64MiB 3,3V 8-bit",	0xF2, 64},
	{"NAND 64MiB 3,3V 8-bit",	0xD0, 64},
	{"NAND 64MiB 1,8V 16-bit",	0xB2, 64},
	{"NAND 64MiB 1,8V 16-bit",	0xB0, 64},
	{"NAND 64MiB 3,3V 16-bit",	0xC2, 64},
	{"NAND 64MiB 3,3V 16-bit",	0xC0, 64},

	/* 1 Gigabit */
	{"NAND 128MiB 1,8V 8-bit",	0xA1,128},
	{"NAND 128MiB 3,3V 8-bit",	0xF1,128},
	{"NAND 128MiB 3,3V 8-bit",	0xD1,128},
	{"NAND 128MiB 1,8V 16-bit",	0xB1,128},
	{"NAND 128MiB 3,3V 16-bit",	0xC1,128},
	{"NAND 128MiB 1,8V 16-bit", 0xAD,128},

	/* 2 Gigabit */
	{"NAND 256MiB 1.8V 8-bit",	0xAA,256},
	{"NAND 256MiB 3.3V 8-bit",	0xDA,256},
	{"NAND 256MiB 1.8V 16-bit",	0xBA,256},
	{"NAND 256MiB 3.3V 16-bit",	0xCA,256},

	/* 4 Gigabit */
	{"NAND 512MiB 1.8V 8-bit",	0xAC,512},
	{"NAND 512MiB 3.3V 8-bit",	0xDC,512},
	{"NAND 512MiB 1.8V 16-bit",	0xBC,512},
	{"NAND 512MiB 3.3V 16-bit",	0xCC,512},

	/* 8 Gigabit */
	{"NAND 1GiB 1.8V 8-bit",	0xA3,1024},
	{"NAND 1GiB 3.3V 8-bit",	0xD3,1024},
	{"NAND 1GiB 1.8V 16-bit",	0xB3,1024},
	{"NAND 1GiB 3.3V 16-bit",	0xC3,1024},

	/* 16 Gigabit */
	{"NAND 2GiB 1.8V 8-bit",	0xA5,2048},
	{"NAND 2GiB 3.3V 8-bit",	0xD5,2048},
	{"NAND 2GiB 1.8V 16-bit",	0xB5,2048},
	{"NAND 2GiB 3.3V 16-bit",	0xC5,2048},

	/* 32 Gigabit */
	{"NAND 4GiB 1.8V 8-bit",	0xA7,4096},
	{"NAND 4GiB 3.3V 8-bit",	0xD7,4096},
	{"NAND 4GiB 1.8V 16-bit",	0xB7,4096},
	{"NAND 4GiB 3.3V 16-bit",	0xC7,4096},

	/* 64 Gigabit */
	{"NAND 8GiB 1.8V 8-bit",	0xAE,8192},
	{"NAND 8GiB 3.3V 8-bit",	0xDE,8192},
	{"NAND 8GiB 1.8V 16-bit",	0xBE,8192},
	{"NAND 8GiB 3.3V 16-bit",	0xCE,8192},

	/* 128 Gigabit */
	{"NAND 16GiB 1.8V 8-bit",	0x1A,16384},
	{"NAND 16GiB 3.3V 8-bit",	0x3A,16384},
	{"NAND 16GiB 1.8V 16-bit",	0x2A,16384},
	{"NAND 16GiB 3.3V 16-bit",	0x4A,16384},
                                                  
	/* 256 Gigabit */
	{"NAND 32GiB 1.8V 8-bit",	0x1C,32768},
	{"NAND 32GiB 3.3V 8-bit",	0x3C,32768},
	{"NAND 32GiB 1.8V 16-bit",	0x2C,32768},
	{"NAND 32GiB 3.3V 16-bit",	0x4C,32768},

	/* 512 Gigabit */
	{"NAND 64GiB 1.8V 8-bit",	0x1E,65536},
	{"NAND 64GiB 3.3V 8-bit",	0x3E,65536},
	{"NAND 64GiB 1.8V 16-bit",	0x2E,65536},
	{"NAND 64GiB 3.3V 16-bit",	0x4E,65536},
	{0,0,0},
};


struct  {
  unsigned int id;
  char* name;
}  nand_manuf_ids[] = {
	{0x98, "Toshiba"},
	{0xec, "Samsung"},
	{0x04, "Fujitsu"},
	{0x8f, "National"},
	{0x07, "Renesas"},
	{0x20, "ST Micro"},
	{0xad, "Hynix"},
	{0x2c, "Micron"},
	{0xc8, "Elite Semiconductor"},
	{0x01, "Spansion/AMD"},
	{0xef, "Winbond"},
	{0x0, 0}
};

mempoke(nand_cmd,0x8000b); // Extended Fetch ID command
mempoke(nand_exec,1); 
nandwait();
nandid=mempeek(NAND_FLASH_READ_ID); // Get the flash memory ID
chipsize=0;

fid=(nandid>>8)&0xff;
pid=nandid&0xff;

// Determine the manufacturer of the flash memory
i=0;
while (nand_manuf_ids[i].id != 0) {
  if (nand_manuf_ids[i].id == pid) {
  strcpy(flash_mfr,nand_manuf_ids[i].name);
  break;
  }
i++;
}  
    
// Determine the capacity of the flash memory
i=0;
while (nand_ids[i].id != 0) {
if (nand_ids[i].id == fid) {
  chipsize=nand_ids[i].chipsize;
  strcpy(flash_descr,nand_ids[i].type);
  break;
  }
i++;
}  
if (chipsize == 0) {
  printf("\nUndefined Flash ID = %02x",fid);
}  

// Extract configuration parameters

cfg0=mempeek(nand_cfg0);
cfg1=mempeek(nand_cfg1);
ecccfg=mempeek(nand_ecc_cfg);
sectorsize=512;
//sectorsize=(cfg0&(0x3ff<<9))>>9; //UD_SIZE_BYTES = blocksize

devcfg = (nandid>>24) & 0xff;
pagesize = 1024 << (devcfg & 0x3); // Page size in bytes
blocksize = 64 << ((devcfg >> 4) & 0x3);  // Block size in kilobytes
spp = pagesize/sectorsize; // Sectors per page

if ((((cfg0>>6)&7)|((cfg0>>2)&8)) == 0) {
  // For older chipsets, the lower 2 bytes of CFG0 need manual configuration
  if (!bad_loader) mempoke(nand_cfg0,(cfg0|0x40000|(((spp-1)&8)<<2)|(((spp-1)&7)<<6)));
}  

// Determine ECC type and size
if (((cfg1>>27)&1) != 0) bch_mode=1;
if (bch_mode) { 
  // For BCH 
  ecc_size=(ecccfg>>8)&0x1f; 
  ecc_bit=((ecccfg>>4)&3) ? (((ecccfg>>4)&3)+1)*4 : 4;
}
else {
  // For R-S
  ecc_size=(cfg0>>19)&0xf;
  ecc_bit=4;
}  

badposition=(cfg1>>6)&0x3ff;
badplace=(cfg1>>16)&1;

linuxcwsize=528;
if (bch_mode && (ecc_bit == 8)) linuxcwsize=532;

// Configure bad block marker if not auto-configured
c_badmark_pos = (pagesize-(linuxcwsize*(spp-1))+1);
if (badposition == 0) {
  printf("\n! Attention - the position of bad block marker has been auto-configured!\n");  
  badplace=0;
  badposition=c_badmark_pos;
}  
if (badposition != c_badmark_pos) {
  printf("\n! Attention - the current bad block marker position %x does not match the calculated position %x!\n",
     badposition,c_badmark_pos);  
}

// Check for the 16-bit flash memory indicator
if ((cfg1&2) != 0) flash16bit=1;
if (chipsize != 0)   maxblock=chipsize*1024/blocksize;
else                 maxblock=0x800;

if (oobsize == 0) {
  // Micron MT29F4G08ABBEA3W and Toshiba MD5N04G02GSD2ARK:
  // actually 224, determined 128, actually 
  // uses 160, for raw mode, 256 is more intuitive :)
  if ((nandid == 0x2690ac2c) || (nandid == 0x2690ac98)) oobsize = 256; 
  else oobsize = (8 << ((devcfg >> 2) & 0x1)) * (pagesize >> 9);
}
}

//**********************************************
//* Disable Hardware Bad Block Control
//**********************************************
void hardware_bad_off() {
  int cfg1;
  cfg1 = mempeek(nand_cfg1);
  cfg1 &= ~(0x3ff << 6);
  mempoke(nand_cfg1, cfg1);
}

//**********************************************
//* Enable Hardware Bad Block Control
//**********************************************
void hardware_bad_on() {
  int cfg1;
  cfg1 = mempeek(nand_cfg1);
  cfg1 &= ~(0x7ff << 6);
  cfg1 |= (badposition & 0x3ff) << 6; // Offset to the marker
  cfg1 |= badplace << 16;            // Area where the marker is located (user/spare)
  mempoke(nand_cfg1, cfg1);
}

//**********************************************
//* Set Bad Block Marker Position
//**********************************************
void set_badmark_pos(int pos, int place) {
  badposition = pos;
  badplace = place & 1;
  hardware_bad_on();
}

//**********************************
//* Close the Data Stream of the Partition
//**********************************
int qclose(int errmode) {
  unsigned char iobuf[600];
  unsigned char cmdbuf[] = {0x15};
  int iolen;

  iolen = send_cmd(cmdbuf, 1, iobuf);
  if (!errmode) return 1;
  if (iobuf[1] == 0x16) return 1;
  show_errpacket("close()", iobuf, iolen);
  return 0;
}

//************************
//* Erase a Flash Memory Block
//************************
void block_erase(int block) {
  int oldcfg;
  nand_reset();
  mempoke(nand_addr0, block * ppb);         // Lower part of the address - # page
  mempoke(nand_addr1, 0);                 // Upper part of the address - always 0
  oldcfg = mempeek(nand_cfg0);
  mempoke(nand_cfg0, oldcfg & ~(0x1c0));    // Set CW_PER_PAGE=0, as required by the datasheet
  mempoke(nand_cmd, 0x3a); // Erase. Last page bit is set
  mempoke(nand_exec, 0x1);
  nandwait();
  mempoke(nand_cfg0, oldcfg);   // Restore CFG0
}

//****************************************
//* Disable NANDc BAM
//****************************************
void disable_bam() {
  unsigned int i, nandcstate[256], bcraddr = 0xfc401a40;
  if (is_chipset("MDM9x4x")) bcraddr = 0x0183f000;
  for (i = 0; i < 0xec; i += 4) nandcstate[i] = mempeek(nand_cmd + i); // Save the controller's state
  mempoke(bcraddr, 1); // GCC_QPIC_BCR
  mempoke(bcraddr, 0); // Full asynchronous reset of QPIC
  for (i = 0; i < 0xec; i += 4) mempoke(nand_cmd + i, nandcstate[i]);  // Restore the state
  mempoke(nand_exec, 1); // Dummy read to release the protection of controller's address registers
}

//****************************************************
//* Check an Array for Non-Zero Values
//*
//*  0 - the array contains only zeros
//*  1 - the array contains non-zeros
//****************************************************
int test_zero(unsigned char* buf, int len) {
  int i;
  for (i = 0; i < len; i++) {
    if (buf[i] != 0) {
      return 1;
    }
  }
  return 0;
}

//***************************************************************
//* Identify the Chipset through the Applet by the Loader Signature
//*
//* Return -1 - the loader does not support command 11
//*         0 - no chipset identification signature found in the loader
//*         Others - the chipset code from the loader
//***************************************************************
int identify_chipset() {
  char cmdbuf[] = {
    0x11, 0x00, 0x04, 0x10, 0x2d, 0xe5, 0x0e, 0x00, 0xa0, 0xe1, 0x03, 0x00, 0xc0, 0xe3, 0xff, 0x30,
    0x80, 0xe2, 0x34, 0x10, 0x9f, 0xe5, 0x04, 0x20, 0x90, 0xe4, 0x01, 0x00, 0x52, 0xe1, 0x03, 0x00,
    0x00, 0x0a, 0x03, 0x00, 0x50, 0xe1, 0xfa, 0xff, 0xff, 0x3a, 0x00, 0x00, 0xa0, 0xe3, 0x00, 0x00,
    0x00, 0xea, 0x00, 0x00, 0x90, 0xe5, 0x04, 0x10, 0x9d, 0xe4, 0x01, 0x00, 0xc1, 0xe5, 0xaa, 0x00,
    0xa0, 0xe3, 0x00, 0x00, 0xc1, 0xe5, 0x02, 0x40, 0xa0, 0xe3, 0x1e, 0xff, 0x2f, 0xe1, 0xef, 0xbe,
    0xad, 0xde
  };
  unsigned char iobuf[1024];
  send_cmd(cmdbuf, sizeof(cmdbuf), iobuf);
  if (iobuf[1] != 0xaa) return -1;
  return iobuf[2];
}

//*******************************************************
//* Check the Loader Patch Operation
//*
//* Return 0 if command 11 is not supported and set the global variable bad_loader = 1
//*******************************************************
int test_loader() {
  int i = identify_chipset();
  //printf("\n ident = %i\n",i);
  if (i <= 0) {
    bad_loader = 1;
    return 0;
  }
  if (chip_type < 0) {
    set_chipset(i); // Set the chipset if it was not explicitly defined
  }
  return 1;
}

//****************************************************************
//* Check the Bad Block Flag of the Previous Read Operation
//*
//* 0 - no bad block
//* 1 - there is a bad block
//****************************************************************
int test_badblock() {
  unsigned int st, r, badflag = 0;
  // The upper 2 bytes of the nand_buffer_status register reflect the marker read from flash memory.
  // For 8-bit flash chips, only the least significant byte is used, while for 16-bit chips, both bytes are used.
  st = r = mempeek(nand_buffer_status) & 0xffff0000;
  if (flash16bit == 0) {
    if (st != 0xff0000) {
      badflag = 1;
	  //printf("\nst=%08x",r);  
    }
  } else if (st != 0xffff0000) {
    badflag = 1;
  }
  return badflag;
}

//*********************************
//* Check Block for Defects
//*********************************
int check_block(int blk) {
  nand_reset(); // Reset
  setaddr(blk, 0);
  mempoke(nand_cmd, 0x34); // Read data + ecc + spare
  mempoke(nand_exec, 0x1);
  nandwait();
  return test_badblock();
}

//*********************************
//* Write Bad Block Marker
//*********************************
void write_badmark(unsigned int blk, int val) {
  char buf[1000];
  const int udsize = 0x220;
  int i;
  unsigned int cfg1bak, cfgeccbak;
  cfg1bak = mempeek(nand_cfg1);
  cfgeccbak = mempeek(nand_ecc_cfg);
  mempoke(nand_ecc_cfg, mempeek(nand_ecc_cfg) | 1);
  mempoke(nand_cfg1, mempeek(nand_cfg1) | 1);
  hardware_bad_off();
  memset(buf, val, udsize);
  buf[0] = 0xeb;   // Marker of artificially created bad block
  nand_reset();
  nandwait();
  setaddr(blk, 0);
  mempoke(nand_cmd, 0x39); // Write data + ecc + spare
  for (i = 0; i < spp; i++) {
    memwrite(sector_buf, buf, udsize);
    mempoke(nand_exec, 1);
    nandwait();
  }
  hardware_bad_on();
  mempoke(nand_cfg1, cfg1bak);
  mempoke(nand_ecc_cfg, cfgeccbak);
}

//************************************************
//* Set a Bad Block Marker
//* -> 0 - the block was already defective
//*    1 - it was normal and made defective
//**********************************************
int mark_bad(unsigned int blk) {
  // flash_read(blk, 0, 0);
  if (!check_block(blk)) {
    write_badmark(blk, 0);
    return 1;
  }
  return 0;
}

//************************************************
//* Remove a Bad Block Marker
//* -> 0 - the block was not defective
//*    1 - it was defective and made normal
//************************************************
int unmark_bad(unsigned int blk) {
  // flash_read(blk, 0, 0);
  if (check_block(blk)) {
    block_erase(blk);
    return 1;
  }
  return 0;
}

//****************************************************
//* Check the Buffer for the Bad Block Filler
//****************************************************
int test_badpattern(unsigned char* buf) {
  int i;
  for (i = 0; i < 512; i++) {
    if (buf[i] != 0xbb) return 0;
  }
  return 1;
}

//**********************************************************
//* Set the Size of the Data Field of a Sector
//**********************************************************
void set_udsize(unsigned int size) {
  unsigned int tmpreg = mempeek(nand_cfg0);
  tmpreg = (tmpreg & (~(0x3ff << 9))) | (size << 9); // CFG0.UD_SIZE_BYTES
  mempoke(nand_cfg0, tmpreg);
  if (((mempeek(nand_cfg1) >> 27) & 1) != 0) { // BCH ECC
    tmpreg = mempeek(nand_ecc_cfg);
    tmpreg = (tmpreg & (~(0x3ff << 16)) | (size << 16)); // ECC_CFG.ECC_NUM_DATA_BYTES
    mempoke(nand_ecc_cfg, tmpreg);
  }
}

//**********************************************************
//* Set the Size of the Spare Field
//**********************************************************
void set_sparesize(unsigned int size) {
  unsigned int cfg0 = mempeek(nand_cfg0);
  cfg0 = cfg0 & (~(0xf << 23)) | (size << 23); // SPARE_SIZE_BYTES
  mempoke(nand_cfg0, cfg0);
}

//**********************************************************
//* Set the Size of the ECC Field
//**********************************************************
void set_eccsize(unsigned int size) {
  uint32 cfg0, cfg1, ecccfg, bch_mode = 0;
  cfg1 = mempeek(nand_cfg1);
  // Determine the ECC type
  if (((cfg1 >> 27) & 1) != 0) bch_mode = 1;
  if (bch_mode) {
    ecccfg = mempeek(nand_ecc_cfg);
    ecccfg = (ecccfg & (~(0x1f << 8)) | (size << 8));
    mempoke(nand_ecc_cfg, ecccfg);
  } else {
    cfg0 = mempeek(nand_cfg0);
    cfg0 = cfg0 & (~(0xf << 19)) | (size << 19); // ECC_PARITY_SIZE_BYTES = eccs
    mempoke(nand_cfg0, cfg0);
  }
}

//**********************************************************
//* Set the Sector Format in the Controller Configuration
//*
//* udsize - data size in bytes
//* ss - spare size in unknown units
//* eccs - ECC size in bytes
//**********************************************************
void set_blocksize(unsigned int udsize, unsigned int ss, unsigned int eccs) {
  set_udsize(udsize);
  set_sparesize(ss);
  set_eccsize(eccs);
}

//******************************************************************
//* Get the Current udsize
//******************************************************************
int get_udsize() {
  return (mempeek(nand_cfg0) & (0x3ff << 9)) >> 9;
}

//******************************************************************
//* Parse the parameters of the key determining the position of the bad marker
//*
//* Parameter format:
//*   xxx  - marker in the data area of the sector
//*   Uxxx - marker in the data area of the sector
//*   Sxxx - marker in the OOB area (spare)
//*
//* badpos - marker position
//* badloc - area where the marker is located (0-user, 1-spare)
//******************************************************************
void parse_badblock_arg(char* arg, int* badpos, int* badloc) {
  char* str = arg;
  *badloc = 0;
  if (toupper(str[0]) == 'U') str++;
  else if (toupper(str[0]) == 'S') {
    *badloc = 1;
    str++;
  }
  sscanf(str, "%x", badpos);
}

//***************************************************************
//* Determine the ECC Correction Status after a Read Operation
//*
//* Returns:
//*  0 - there was no correction
//* -1 - uncorrectable error
//* >0 - number of corrected errors
//***************************************************************
int check_ecc_status() {
  int bs;
  bs = mempeek(nand_buffer_status);
  if (((bs & 0x100) != 0) && ((mempeek(nand_cmd + 0xec) & 0x40) == 0)) return -1; // uncorrectable error
  return bs & 0x1f; // number of correctable errors
}

//***************************************************************
//* Reset the ECC Engine VSN
//***************************************************************
void bch_reset() {
  int cfgecctemp;
  if (!bch_mode) return;
  cfgecctemp = mempeek(nand_ecc_cfg); // Configuration considering ECC enable/disable
  mempoke(nand_ecc_cfg, cfgecctemp | 2); // Reset BCH engine
  mempoke(nand_ecc_cfg, cfgecctemp); // Restore BCH configuration
}
