#include "include.h"

int test_empty(unsigned char* srcbuf)
{
    for(int i = 0; i < pagesize; i++)
        if(srcbuf[i] != 0xff)
            return 0;
    return 1;
}

//**********************************************************
//*   Set the chipset to Linux format for flash partitions
//**********************************************************
void set_linux_format() {
  
unsigned int sparnum, cfgecctemp;

if (nand_ecc_cfg != 0xffff) cfgecctemp=mempeek(nand_ecc_cfg);
else cfgecctemp=0;
sparnum = 6-((((cfgecctemp>>4)&3)?(((cfgecctemp>>4)&3)+1)*4:4)>>1);
// For ECC- R-S
if (! (is_chipset("MDM9x25") || is_chipset("MDM9x3x") || is_chipset("MDM9x4x"))) set_blocksize(516,1,10); // data - 516, spare - 1 byte, ecc - 10
// For ECC - BCH
else {
	set_udsize(516); // data - 516, spare - 2 or 4 bytes
	set_sparesize(sparnum);
}
}

//*******************************************
//@@@@@@@@@@@@ Main program
void main(int argc, char* argv[]) {
  
unsigned char datacmd[1024]; // Sector buffer
unsigned char srcbuf[8192]; // Page buffer
unsigned char membuf[1024]; // Verification buffer
unsigned char databuf[8192], oobuf[8192]; // Data and OOB sector buffers
unsigned int fsize;
FILE* in;
int vflag=0;
int sflag=0;
int cflag=0;
unsigned int flen=0;
#ifndef WIN32
char devname[]="/dev/ttyUSB0";
#else
char devname[20]="";
#endif
unsigned int cfg0bak,cfg1bak,cfgeccbak,cfgecctemp;
unsigned int i,opt;
unsigned int block,page,sector;
unsigned int startblock=0;
unsigned int stopblock = -1;
unsigned int bsize;
unsigned int fileoffset=0;
int badflag;
int uxflag=0, ucflag=0, usflag=0, umflag=0, ubflag=0;
int wmode=0; // Write mode
int readlen;

#define w_standart 0
#define w_linux    1
#define w_yaffs    2
#define w_image    3
#define w_linout   4

while ((opt = getopt(argc, argv, "hp:k:b:a:f:vc:z:l:o:u:s")) != -1) {
  switch (opt) {
   case 'h': 
    printf("\n  The utility is intended for writing a raw flash image through controller registers\n\
Available options are:\n\n\
-p <tty>  - specifies the name of the serial port device for communication with the bootloader\n\
-k #      - chipset code (-kl - get the list of codes)\n\
-b #      - starting block number for writing\n\
-a #      - block number on the flash until which writing is allowed (default is the end of the flash)\n\
-f <x>    - choose the write format:\n\
        -fs (default) - write only data sectors\n\
        -fl - write data sectors in Linux format\n\
        -fy - write yaffs2 images\n\
	-fi - write a raw image with data+OOB as is, without ECC recalculation\n\
	-fo - data-only input and Linux format on the flash\n");
printf("\
-s        - skip pages containing only 0xff bytes (necessary for writing ubi images)\n\
-z #      - OOB size per page in bytes (overrides auto-detected size)\n\
-l #      - number of blocks to write, default - until the end of the input file\n\
-o #      - offset in blocks in the source file to the beginning of the writable area\n\
-ux       - disable hardware control of bad blocks\n\
-us       - ignore bad block markers in the input file\n\
-uc       - simulate bad blocks in the input file\n\
-um       - check for consistency of bad blocks in the file and flash\n\
-ub       - do not check the flash block's badness before writing (DANGEROUS!)\n\
-v        - verify the written data after writing\n\
-c n      - only erase n blocks, starting from the initial block.\n\
\n");
    return;
    
   case 'k':
    define_chipset(optarg);
    break;

   case 'p':
    strcpy(devname,optarg);
    break;
    
   case 'c':
     sscanf(optarg,"%x",&cflag);
     if (cflag == 0) {
       printf("\n Invalid argument for the -c option");
       return;
     }  
     break;
     
   case 'b':
     sscanf(optarg,"%x",&startblock);
     break;

   case 'a':
     sscanf(optarg,"%x",&stopblock);
     break;
     
   case 'z':
     sscanf(optarg,"%u",&oobsize);
     break;
     
   case 'l':
     sscanf(optarg,"%x",&flen);
     break;
     
   case 'o':
     sscanf(optarg,"%x",&fileoffset);
     break;
     
   case 's':
     sflag=1;
     break;
     
   case 'v':
     vflag=1;
     break;
     
   case 'f':
     switch(*optarg) {
       case 's':
        wmode=w_standart;
	break;
	
       case 'l':
        wmode=w_linux;
	break;
	
       case 'y':
        wmode=w_yaffs;
	break;
	
       case 'i':
        wmode=w_image;
	break;

       case 'o':
        wmode=w_linout;
	break;
	
       default:
	printf("\n Invalid value for the -f option\n");
	return;
     }
     break;
     
   case 'u':  
     switch (*optarg) {
       case 'x':
         uxflag=1;
	 break;
	 
       case 's':
         usflag=1;
	 break;
	 
       case 'c':
         ucflag=1;
	 break;
	 
       case 'm':
         umflag=1;
	 break;
	 
       case 'b':
         ubflag=1;
	 break;
	 
       default:
	printf("\n Invalid value for the -u option\n");
	return;
     }
     break;
     
   case '?':
   case ':':  
     return;
  }
}  

if (uxflag+usflag+ucflag+umflag > 1) {
  printf("\n Options -ux, -us, -uc, -um are not compatible with each other\n");
  return;
}

if (uxflag+ubflag+umflag > 1) {
  printf("\n Options -ux, -ub, -um are not compatible with each other\n");
  return;
}

if (uxflag+ubflag > 1) {
  printf("\n Options -ux, -ub, -um are not compatible with each other\n");
  return;
}

if (uxflag && (wmode != w_image)) {
  printf("\n Option -ux is only valid in -fi mode\n");
  return;
}

#ifdef WIN32
if (*devname == '\0')
{
   printf("\n Serial port is not specified\n"); 
   return; 
}
#endif

if (!open_port(devname))  {
#ifndef WIN32
   printf("\n Serial port %s cannot be opened\n", devname); 
#else
   printf("\n Serial port COM%s cannot be opened\n", devname); 
#endif
   return; 
}

if (!cflag) { 
 in=fopen(argv[optind],"rb");
 if (in == 0) {
   printf("\nError opening the input file\n");
   return;
 }
 
}
else if (optind < argc) {// No input file is needed in erase mode
  printf("\n The -c option is not compatible with an input file\n");
  return;
}

if(stopblock <= startblock) {
  printf("\n The -b option value should be less than the -a option value\n");
  return;
}

hello(0);

if(stopblock == -1)
    stopblock = maxblock;

if(maxblock < startblock) {
  printf("\n Error, the -b option value is greater than the flash memory size\n");
  return;
}

if(maxblock < stopblock) {
  printf("\n Error, the -a option value is greater than the flash memory size\n");
  return;
}

if ((wmode == w_standart)||(wmode == w_linux)) oobsize=0; // For input files without OOB
oobsize/=spp;   // Now oobsize is the size of OOB per sector

// Reset the NAND controller
nand_reset();

// Saving the controller register values
cfg0bak = mempeek(nand_cfg0);
cfg1bak = mempeek(nand_cfg1);
cfgeccbak = mempeek(nand_ecc_cfg);

//-------------------------------------------
// Erase mode
//-------------------------------------------
if (cflag) {
  if ((startblock + cflag) > stopblock) cflag = stopblock - startblock;
  printf("\n");
  for (block = startblock; block < (startblock + cflag); block++) {
    printf("\r Erasing block %03x", block);
    if (!ubflag)
      if (check_block(block)) {
        printf(" - bad block, erasure prohibited\n");
        continue;
      }
    block_erase(block);
  }
  printf("\n");
  return;
}

// ECC on-off
if (wmode != w_image) {
  mempoke(nand_ecc_cfg, mempeek(nand_ecc_cfg) & 0xfffffffe);
  mempoke(nand_cfg1, mempeek(nand_cfg1) & 0xfffffffe);
}
else {
  mempoke(nand_ecc_cfg, mempeek(nand_ecc_cfg) | 1);
  mempoke(nand_cfg1, mempeek(nand_cfg1) | 1);
}

// Determine the file size
if (wmode == w_linout) bsize = pagesize * ppb; // For this mode, the file does not contain OOB data, but OOB writing is required
else bsize = (pagesize + oobsize * spp) * ppb;  // Size in bytes of the complete flash block, including OOB
fileoffset *= bsize; // Convert the offset from blocks to bytes
fseek(in, 0, SEEK_END);
i = ftell(in);
if (i <= fileoffset) {
  printf("\n Offset %i exceeds the file boundaries\n", fileoffset / bsize);
  return;
}
i -= fileoffset; // Trim the size of the file based on the skipped portion
fseek(in, fileoffset, SEEK_SET); // Move to the beginning of the writeable section
fsize = i / bsize; // Size in blocks
if ((i % bsize) != 0) fsize++; // Round up to the block boundary if necessary

if (flen == 0) flen = fsize;
else if (flen > fsize) {
  printf("\n The specified length %u exceeds the file size %u\n", flen, fsize);
  return;
}

printf("\n Writing from the file %s, start block %03x, size %03x\n Write mode: ", argv[optind], startblock, flen);

switch (wmode) {
  case w_standart:
    printf("data only, standard format\n");
    break;

  case w_linux:
    printf("data only, Linux format on input\n");
    break;

  case w_image:
    printf("raw image without ECC calculation\n");
    printf(" Data format: %u+%u\n", sectorsize, oobsize);
    break;

  case w_yaffs:
    printf("YAFFS2 image\n");
    set_linux_format();
    break;

  case w_linout:
    printf("Linux format on the flash\n");
    set_linux_format();
    break;
}

port_timeout(1000);

// Block loop
if ((startblock + flen) > stopblock) flen = stopblock - startblock;
for (block = startblock; block < (startblock + flen); block++) {
  // Check for block defects if needed
  badflag = 0;
  if (!uxflag && !ubflag)  badflag = check_block(block);
  // Target block is defective
  if (badflag) {
	//printf("\n %x - badflag\n",block);
    // Skip the defective block and continue
    if (!ubflag && !(umflag || ucflag)) {
      printf("\r Block %x is defective - skipping\n", block);
      if (startblock + flen == stopblock)
          printf("\n Attention: the last block of the file will not be written, exceeding the write area boundary\n\n");
      else
          flen++;   // Shift the end boundary of the input file - we skipped a block, data is spreading
      continue;
    }
  }
  // Erase the block
  if (!badflag || ubflag) {
    block_erase(block);
  }

  bch_reset();

  // Page loop
  for (page = 0; page < ppb; page++) {

    memset(oobuf, 0xff, sizeof(oobuf));
    memset(srcbuf, 0xff, pagesize); // Fill the buffer with FF for reading incomplete pages
    // Read the entire page dump
    if (wmode == w_linout) readlen = fread(srcbuf, 1, pagesize, in);
    else readlen = fread(srcbuf, 1, pagesize + (spp * oobsize), in);
    if (readlen == 0) goto endpage;  // 0 - all data from the file has been read

    if (sflag && test_empty(srcbuf))
    {
        //printf("\n Skipped empty page: block %x, page %x\n", block, page);
        continue;
    }

    // srcbuf is read - check if it's a bad block
    if (test_badpattern(srcbuf)) {
        // It's indeed a bad block
        if (usflag)
        {
            // If it's said to write as is, we write it as is
            if(page == 0)
                printf("\r Block %x - bad block flag written \"as is,\" continue writing\n", block);
        }
        else if (!badflag)
        {
            // The input block doesn't match the bad block on the flash, or we don't care what's on the flash
            if (ucflag)
            {
                if(page == 0)
                {
                    // Creating a bad block
                    // todo: rollback in case of error
                    mark_bad(block);
                    printf("\r Block %x marked as defective according to the input file!\n", block);
                }
                continue;
            }
            else if (umflag)
            {
                printf("\r Block %x: no defect found on flash, exiting!\n", block);
                return;
            }
            else
            {
	        if (page == 0)
                    printf("\r Bad block flag detected in the input dump - skipping\n");
	        continue;  // Skip this block, page by page
            }
        }
        else
        {
            // If we don't care about flash bad blocks and there is a bad block indeed
            if (page == 0)
                printf("\r Block %x - defects match, continue writing\n", block);
	    continue;  // Skip this block, page by page
        }
    }
if (badflag) {
    printf("\r Block %x: Unexpected defect detected on the flash, exiting!\n", block);
    return;
}

// Process the dump into buffers
switch (wmode) {
  case w_standart:
  case w_linux:
  case w_image:
    // For all modes except yaffs and linout - input file format is 512+oob
    for (i = 0; i < spp; i++) {
      memcpy(databuf + sectorsize * i, srcbuf + (sectorsize + oobsize) * i, sectorsize);
      if (oobsize != 0) memcpy(oobuf + oobsize * i, srcbuf + (sectorsize + oobsize) * i + sectorsize, oobsize);
    }
    break;

  case w_yaffs:
    // For yaffs mode - input file format is pagesize+oob
    memcpy(databuf, srcbuf, sectorsize * spp);
    memcpy(oobuf, srcbuf + sectorsize * spp, oobsize * spp);
    break;

  case w_linout:
    // For this mode - the input file only contains data with a size of pagesize
    memcpy(databuf, srcbuf, pagesize);
    break;
}

// Set the flash address
printf("\r Block: %04x   Page: %02x", block, page);
fflush(stdout);
setaddr(block, page);

// Set the write command code
switch (wmode) {
  case w_standart:
    mempoke(nand_cmd, 0x36); // Page program - write only the block body
    break;

  case w_linux:
  case w_yaffs:
  case w_linout:
    mempoke(nand_cmd, 0x39); // Write data+spare, ECC is calculated by the controller
    break;

  case w_image:
    mempoke(nand_cmd, 0x39); // Write data+spare+ecc, all data from the buffer goes directly to the flash
    break;
}

// Sector loop
for (sector = 0; sector < spp; sector++) {
  memset(datacmd, 0xff, sectorsize + 64); // Fill the sector buffer with FF - default values

  // Fill the sector buffer with data
  switch (wmode) {
    case w_linux:
      // Linux (Chinese weird) data layout, writing without OOB
      if (sector < (spp - 1))
        // First n sectors
        memcpy(datacmd, databuf + sector * (sectorsize + 4), sectorsize + 4);
      else
        // Last sector
        memcpy(datacmd, databuf + (spp - 1) * (sectorsize + 4), sectorsize - 4 * (spp - 1)); // Shorten the last sector
      break;

    case w_standart:
      // Standard format - only 512-byte sectors, without OOB
      memcpy(datacmd, databuf + sector * sectorsize, sectorsize);
      break;

    case w_image:
      // Raw image - data+oob, ECC is not calculated
      memcpy(datacmd, databuf + sector * sectorsize, sectorsize); // Data
      memcpy(datacmd + sectorsize, oobuf + sector * oobsize, oobsize); // OOB
      break;

    case w_yaffs:
      // YAFFS Image - Write only data in 516-byte blocks and a YAFFS tag at the end of the last block
      // The input file is in the format of page+oob, but the tag is located at position 0 in the OOB area.
      if (sector < (spp - 1))
        // First n sectors
        memcpy(datacmd, databuf + sector * (sectorsize + 4), sectorsize + 4);
      else {
        // Last sector
        memcpy(datacmd, databuf + (spp - 1) * (sectorsize + 4), sectorsize - 4 * (spp - 1)); // Shorten the last sector
        memcpy(datacmd + sectorsize - 4 * (spp - 1), oobuf, 16); // Append the yaffs tag to it
      }
      break;

    case w_linout:
      // Write only data with 516-byte blocks
      if (sector < (spp - 1))
        // First n sectors
        memcpy(datacmd, databuf + sector * (sectorsize + 4), sectorsize + 4);
      else {
        // Last sector
        memcpy(datacmd, databuf + (spp - 1) * (sectorsize + 4), sectorsize - 4 * (spp - 1)); // Shorten the last sector
      }
      break;
  }
  // Transfer the sector to the sector buffer
  if (!memwrite(sector_buf, datacmd, sectorsize + oobsize)) {
    printf("\n Error transferring the sector buffer\n");
    return;
  }
  // If necessary, disable bad block checking
  if (uxflag) hardware_bad_off();
  // Execute the write command and wait for it to finish
  mempoke(nand_exec, 0x1);
  nandwait();
  // Enable bad block checking again
  if (uxflag) hardware_bad_on();
}  // End of the sector loop

if (!vflag) continue; // Verification is not required

// Data verification
printf("\r");
setaddr(block, page);
mempoke(nand_cmd, 0x34); // Read data+ecc+spare

// Sector verification loop
for (sector = 0; sector < spp; sector++) {
  // Read the next sector
  mempoke(nand_exec, 0x1);
  nandwait();

  // Read the sector buffer
  memread(membuf, sector_buf, sectorsize + oobsize);
  switch (wmode) {
    case w_linux:
      // Verification in Linux format
      if (sector != (spp - 1)) {
        // All sectors except the last one
        for (i = 0; i < sectorsize + 4; i++)
          if (membuf[i] != databuf[sector * (sectorsize + 4) + i])
            printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
              block, page, sector, i, membuf[i], databuf[sector * (sectorsize + 4) + i]);
      } else {
        // Last sector
        for (i = 0; i < sectorsize - 4 * (spp - 1); i++)
          if (membuf[i] != databuf[(spp - 1) * (sectorsize + 4) + i])
            printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
              block, page, sector, i, membuf[i], databuf[(spp - 1) * (sectorsize + 4) + i]);
      }
      break;

    case w_standart:
    case w_image:
    case w_yaffs:
    case w_linout: // Currently not working!
      // Verification in standard format
      for (i = 0; i < sectorsize; i++)
        if (membuf[i] != databuf[sector * sectorsize + i])
          printf("! block: %04x  page:%02x  sector:%u  byte: %03x  %02x != %02x\n",
            block, page, sector, i, membuf[i], databuf[sector * sectorsize + i]);
      break;
  }  // switch(wmode)
}  // End of the sector verification loop
}  // End of the page loop
}  // End of the block loop

endpage:
mempoke(nand_cfg0, cfg0bak);
mempoke(nand_cfg1, cfg1bak);
mempoke(nand_ecc_cfg, cfgeccbak);
printf("\n");
}