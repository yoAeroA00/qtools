//---------------------------------------------------
// Utility for Managing Defective Flash Blocks
//---------------------------------------------------

#include "include.h"

//*********************************
//* Building a list of defective blocks
//*********************************
void defect_list(int start, int len) {
  
  FILE* out; 
  int blk;
  int badcount = 0;
  int pn;

  out = fopen("badblock.lst", "w");
  if (out == 0) {
    printf("\n Unable to create the file badblock.lst\n");
    return;
  }
  fprintf(out, "List of defective blocks");

  // Load the partition table from flash
  load_ptable("@");

  printf("\nBuilding a list of defective blocks in the range %08x - %08x\n", start, start + len);

  // Main loop to check each block
  for (blk = start; blk < (start + len); blk++) {
    printf("\r Checking block %08x", blk);
    fflush(stdout);
    if (check_block(blk)) {
      printf(" - bad block");
      fprintf(out, "\n%08x", blk);
      // Increment the bad block counter
      badcount++;
      // Display the partition name associated with this block
      if (validpart) {
        // Search for the partition that contains this block
        pn = block_to_part(blk);
        if (pn != -1) {
          printf(" (%s+%x)", part_name(pn), blk - part_start(pn));
          fprintf(out, " (%s+%x)", part_name(pn), blk - part_start(pn));
        }
      }
      printf("\n");
    }
  } // blk

  fprintf(out, "\n");
  fclose(out);
  printf("\r * Total defective blocks: %i\n", badcount);
}

//***********************************************************
//* ECC Error Scanning
//* flag=0 - all errors, 1 - only correctable errors
//***********************************************************
void ecc_scan(int start, int len, int flag) {
  
  int blk;
  int errcount = 0;
  int pg, sec;
  int stat;
  FILE* out; 

  printf("\nBuilding a list of ECC errors in the range %08x - %08x\n", start, start + len);

  out = fopen("eccerrors.lst", "w");
  fprintf(out, "List of ECC errors");
  if (out == 0) {
    printf("\n Unable to create the file eccerrors.lst\n");
    return;
  }

  // Enable ECC
  mempoke(nand_ecc_cfg, mempeek(nand_ecc_cfg) & 0xfffffffe); 
  mempoke(nand_cfg1, mempeek(nand_cfg1) & 0xfffffffe); 

  for (blk = start; blk < (start + len); blk++) {
    printf("\r Checking block %08x", blk);
    fflush(stdout);
    if (check_block(blk)) {
      printf(" - bad block\n");
      continue;
    }
    bch_reset(); 
    for (pg = 0; pg < ppb; pg++) {
      setaddr(blk, pg);
      mempoke(nand_cmd, 0x34); // Read data
      for (sec = 0; sec < spp; sec++) {
        mempoke(nand_exec, 0x1);
        nandwait();
        stat = check_ecc_status();
        if (stat == 0) continue;
        if ((stat == -1) && (flag == 1)) continue;
        if (stat == -1) { 
          printf("\r! Block %x  Page %d  Sector %d: Uncorrectable read error\n", blk, pg, sec);
          fprintf(out, "\r! Block %x  Page %d  Sector %d: Uncorrectable read error\n", blk, pg, sec);
        }  
        else {
          printf("\r! Block %x  Page %d  Sector %d: Corrected bit: %d\n", blk, pg, sec, stat);
          fprintf(out, "\r! Block %x  Page %d  Sector %d: Corrected bit: %d\n", blk, pg, sec, stat);
        }  
        errcount++;
      }
    }
  } 
  printf("\r * Total errors: %i\n", errcount);
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char* argv[]) {

  unsigned int start = 0, len = 0, opt;
  int mflag = 0, uflag = 0, sflag = 0;
  int dflag = 0;
  int badloc = 0;
  int eflag = -1;

  #ifndef WIN32
  char devname[50] = "/dev/ttyUSB0";
  #else
  char devname[50] = "";
  #endif

  while ((opt = getopt(argc, argv, "hp:b:l:dm:k:u:s:e:")) != -1) {
    switch (opt) {
      case 'h':
        printf("\n Utility for working with defective flash storage blocks\n\
Supported options:\n\n\
-p <tty> - Serial port for communication with the bootloader\n\
-k # - Chipset code (-kl - list chipset codes)\n\
-b <blk> - Starting block number to read (default is 0)\n\
-l <num> - Number of blocks to read, 0 means until the end of the flash\n\n\
-d - List existing defective blocks\n\
-e# - List ECC errors, #=0 lists all errors, 1 lists only correctable errors\n\
-m blk - Mark block blk as defective\n\
-u blk - Unmark block blk as defective\n\
-s L### - Permanently set the bad block marker position to byte ###, L=U(user) or S(spare)\n\
        ");
        return;

      case 'k':
        define_chipset(optarg);
        break;

      case 'p':
        strcpy(devname, optarg);
        break;

      case 'b':
        sscanf(optarg, "%x", &start);
        break;

      case 'l':
        sscanf(optarg, "%x", &len);
        break;

      case 'm':
        sscanf(optarg, "%x", &mflag);
        break;

      case 'u':
        sscanf(optarg, "%x", &uflag);
        break;

      case 's':
        parse_badblock_arg(optarg, &sflag, &badloc);
        break;

      case 'd':
        dflag = 1;
        break;

      case 'e':
        sscanf(optarg, "%i", &eflag);
        if ((eflag != 0) && (eflag != 1)) {
          printf("\n Invalid -e option value\n");
          return;
        }
        break;

      case '?':
      case ':':
        return;
    }
  }

  if ((eflag != -1) && (dflag != 0)) {
    printf("\n Options -e and -d are not compatible\n");
    return;
  }

  #ifdef WIN32
  if (*devname == '\0') {
    printf("\n - Serial port is not specified\n");
    return;
  }
  #endif

  if (!open_port(devname)) {
    #ifndef WIN32
    printf("\n - Serial port %s cannot be opened\n", devname);
    #else
    printf("\n - Serial port COM%s cannot be opened\n", devname);
    #endif
    return;
  }

  hello(0);

  // Reset all controller operations
  mempoke(nand_cmd, 1);
  mempoke(nand_exec, 0x1);
  nandwait();

  // Set the position of the bad block marker permanently
  if (sflag) set_badmark_pos(sflag, badloc);

  //###################################################
  // Defective blocks list mode:
  //###################################################

  if (dflag) {
    if (len == 0) len = maxblock - start; // Until the end of the flash
    defect_list(start, len);
    return;
  }

  //###################################################
  // ECC errors list mode:
  //###################################################
  if (eflag != -1) {
    if (len == 0) len = maxblock - start; // Until the end of the flash
    ecc_scan(start, len, eflag);
    return;
  }

  //###################################################
  // Marking a block as defective
  //###################################################
  if (mflag) {
    if (mark_bad(mflag)) {
      printf("\n Block %x marked as defective\n", mflag);
    }
    else printf("\n Block %x is already defective\n", mflag);
    return;
  }

  //###################################################
  // Unmarking a block as defective
  //###################################################
  if (uflag) {
    if (unmark_bad(uflag)) {
      printf("\n Marker for block %x removed\n", uflag);
    }
    else printf("\n Block %x is not defective\n", uflag);
    return;
  }
}
