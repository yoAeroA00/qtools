#include "include.h"

//
// Read Flash of the Modem to a File
//

// Flags for handling bad blocks
enum {
    BAD_UNDEF,
    BAD_FILL,
    BAD_SKIP,
    BAD_IGNORE,
    BAD_DISABLE
};

// Formats of the read data
enum {
    RF_AUTO,
    RF_STANDARD,
    RF_LINUX,
    RF_YAFFS
};

int bad_processing_flag = BAD_UNDEF;
unsigned char *blockbuf;

// Structure to store the list of read errors
struct {
    int page;
    int sector;
    int errcode;
} errlist[1000];

int errcount;
int qflag = 0;

//********************************************************************************
//* Load a block into the block buffer
//*
//* Returns 0 if the block is defective, or 1 if it is read successfully
//* cwsize - size of the read sector (including OOB, if needed)
//********************************************************************************
unsigned int load_block(int blk, int cwsize) {
    int pg, sec;
    int oldudsize, cfg0;
    unsigned int cfgecctemp;
    int status;

    errcount = 0;
    if (bad_processing_flag == BAD_DISABLE) hardware_bad_off();
    else if (bad_processing_flag != BAD_IGNORE) {
        if (check_block(blk)) {
            // Defective block detected
            memset(blockbuf, 0xbb, cwsize * spp * ppb); // Fill the block buffer with a placeholder
            return 0; // Return a bad block indicator
        }
    }
    // Good block or we don't care about bad blocks - read the block

    // Set udsize to the size of the read section
    cfg0 = mempeek(nand_cfg0);
//oldudsize=get_udsize();
//set_udsize(cwsize);
//set_sparesize(0);

    nand_reset();
    if (cwsize > (sectorsize + 4)) mempoke(nand_cmd, 0x34); // Read data+ecc+spare without correction
    else mempoke(nand_cmd, 0x33); // Read data with correction

    bch_reset();
    for (pg = 0; pg < ppb; pg++) {
        setaddr(blk, pg);
        for (sec = 0; sec < spp; sec++) {
            mempoke(nand_exec, 0x1);
            nandwait();
            status = check_ecc_status();
            if (status != 0) {
                //printf("\n--- blk %x  pg %i  sec  %i err %i---\n",blk,pg,sec,check_ecc_status());
                errlist[errcount].page = pg;
                errlist[errcount].sector = sec;
                errlist[errcount].errcode = status;
                errcount++;
            }

            memread(blockbuf + (pg * spp + sec) * cwsize, sector_buf, cwsize);
            //dump(blockbuf+(pg*spp+sec)*cwsize,cwsize,0);
        }
    }
    if (bad_processing_flag == BAD_DISABLE) hardware_bad_on();
    //set_udsize(oldudsize);
    mempoke(nand_cfg0,cfg0);
    return 1; // Success - block is read
}

//***************************************
//* Read a block of data to the output file
//***************************************
unsigned int read_block(int block, int cwsize, FILE *out) {
    unsigned int okflag = 0;

    okflag = load_block(block, cwsize);
    if (okflag || (bad_processing_flag != BAD_SKIP)) {
        // The block was read or not read, but we are skipping bad blocks.
        fwrite(blockbuf, 1, cwsize * spp * ppb, out); // Write it to the file
    }
    return !okflag;
}

//********************************************************************************
//* Read a block of data for partitions with protected spare areas (516-byte sectors)
//* yaffsmode = 0: Read data, 1: Read data and yaffs2 tag
//********************************************************************************
unsigned int read_block_ext(int block, FILE *out, int yaffsmode) {
    unsigned int page, sec;
    unsigned int okflag;
    unsigned char *pgoffset;
    unsigned char *udoffset;
    unsigned char extbuf[2048]; // Buffer for assembling pseudo-OOB

    okflag = load_block(block, sectorsize + 4);
    if (!okflag && (bad_processing_flag == BAD_SKIP)) return 1; // Defective block detected

    // Loop through pages
    for (page = 0; page < ppb; page++) {
        pgoffset = blockbuf + page * spp * (sectorsize + 4); // Offset to the current page
        // Loop through sectors
        for (sec = 0; sec < spp; sec++) {
            udoffset = pgoffset + sec * (sectorsize + 4); // Offset to the current sector
            //printf("\n page %i  sector %i\n",page,sec);
            if (sec != (spp - 1)) {
                // For non-last sectors
                fwrite(udoffset, 1, sectorsize + 4, out); // Sector body + 4 bytes OOB
                //dump(udoffset,sectorsize+4,udoffset-blockbuf);
            } else {
                // For the last sector
                fwrite(udoffset, 1, sectorsize - 4 * (spp - 1), out); // Sector body - OOB tail
                //dump(udoffset,sectorsize-4*(spp-1),udoffset-blockbuf);
            }
        }

        // Read yaffs2 tag data
        if (yaffsmode) {
            memset(extbuf, 0xff, oobsize);
            memcpy(extbuf, pgoffset + (sectorsize + 4) * (spp - 1) + (sectorsize - 4 * (spp - 1)), 16);
            //printf("\n page %i oob\n",page);
            //dump(pgoffset+(sectorsize+4)*(spp-1)+(sectorsize-4*(spp-1)),16,pgoffset+(sectorsize+4)*(spp-1)+(sectorsize-4*(spp-1))-blockbuf);
            fwrite(extbuf, 1, oobsize, out);
        }
    }

    return !okflag;
}

//*************************************************************
//* Read a block of data for non-file Linux partitions
//*************************************************************
unsigned int read_block_resequence(int block, FILE *out) {
    return read_block_ext(block, out, 0);
}

//*************************************************************
//* Read a block of data for Yaffs2 file partitions
//*************************************************************
unsigned int read_block_yaffs(int block, FILE *out) {
    return read_block_ext(block, out, 1);
}

//****************************************
//* Display the list of read errors
//****************************************
void show_errlist() {
    int i;

    if (qflag || (errcount == 0)) return; // No errors or silent mode
    for (i = 0; i < errcount; i++) {
        if (errlist[i].errcode == -1) printf("\n! Page %d Sector %d: Uncorrectable read error",
            errlist[i].page, errlist[i].sector);
        else printf("\n! Page %d Sector %d: Corrected bits: %d",
            errlist[i].page, errlist[i].sector, errlist[i].errcode);
    }
    printf("\n");
}

//*****************************
//* Read raw flash
//*****************************
void read_raw(int start, int len, int cwsize, FILE *out, unsigned int rflag) {
    int block;
    unsigned int badflag;

    printf("\n Reading blocks %08x - %08x", start, start + len - 1);
    printf("\n Data format: %u+%i\n", sectorsize, cwsize - sectorsize);

    // Main loop
    // Loop through blocks
    for (block = start; block < (start + len); block++) {
        printf("\r Block: %08x", block);
        fflush(stdout);
        switch (rflag) {
        case RF_AUTO:
        case RF_STANDARD:
            badflag = read_block(block, cwsize, out);
            break;

        case RF_LINUX:
            badflag = read_block_resequence(block, out);
            break;

        case RF_YAFFS:
            badflag = read_block_yaffs(block, out);
            break;
        }
        show_errlist();
        if (badflag != 0) printf(" - Bad block\n");
    }
    printf("\n");
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
void main(int argc, char *argv[]) {

    unsigned char filename[300] = { 0 };
    unsigned int i, block, filepos, lastpos;
    unsigned char c;
    unsigned int start = 0, len = 0, opt;
    unsigned int partlist[60]; // List of partitions allowed for reading
    unsigned int cwsize;  // Size of the data portion read from the sector buffer at once
    FILE *out;
    int partflag = 0;  // 0 - raw flash, 1 - working with the partition table
    int eccflag = 0;  // 1 - disable ECC,  0 - enable
    int partnumber = -1; // Partition selection flag: -1 - all partitions, 1 - by list
    int rflag = RF_AUTO;     // Partition format: 0 - auto, 1 - standard, 2 - Linux, 3- Yaffs2
    int listmode = 0;    // 1- partition map output
    int truncflag = 0;  //  1 - trim all FF from the end of the partition
    int xflag = 0;

    unsigned int badflag;

    int forced_oobsize = -1;

    // Source of the partition table. By default, the MIBIB partition on the flash.
    char ptable_file[200] = "@";

#ifndef WIN32
    char devname[50] = "/dev/ttyUSB0";
#else
    char devname[50] = "";
#endif

    memset(partlist, 0, sizeof(partlist)); // Clear the list of partitions allowed for reading

// Process command line options
while ((opt = getopt(argc, argv, "hp:b:l:o:xs:ef:mtk:r:z:u:q")) != -1) {
    switch (opt) {
    case 'h':
        printf("\n This utility is designed to read a flash memory image using a modified bootloader\n\
 The following options are allowed:\n\n\
-p <tty> - serial port for communication with the bootloader\n\
-e - disable ECC correction during reading\n\
-x - read a full sector - data+oob (without this option, only data is read)\n\n\
-k # - chipset code (-kl - list available codes)\n\
-z # - OOB size per page, in bytes (overrides auto-detected size)\n\
-q   - disable the error reading list output\n\
-u <x> - specify bad block processing mode:\n\
   -uf - fill the image of a bad block in the output file with 0xbb bytes\n\
   -us - skip bad blocks during reading\n\
   -ui - ignore the bad block marker (read as it is)\n\
   -ux - disable hardware bad block control\n");
        printf("\n * For raw reading and bad block checking mode:\n\
-b <blk> - starting block number to read (default is 0)\n\
-l <num> - number of blocks to read, 0 - read until the end of the flash\n\
-o <file> - output file name (default is qflash.bin)(only for reading mode)\n\n\
 * For reading partitions mode:\n\n\
-s <file> - take the partition map from a file\n\
-s @ - take the partition map from the flash (default for -f and -m)\n\
-s - - take the partition map from the file ptable/current-r.bin\n\
-m   - display the full partition map and exit\n\
-f n - read only the partition with number n (can be specified multiple times to form a list of partitions)\n\
-f * - read all partitions\n\
-t - trim all FF bytes after the last meaningful byte of the partition\n\
-r <x> - data format:\n\
    -ra - (default and only for partition mode) auto-detect the format based on the partition attribute\n\
    -rs - standard format (512-byte blocks)\n\
    -rl - Linux format (516-byte blocks, except the last one on the page)\n\
    -ry - Yaffs2 format (page+tag)\n\
");
        return;

    case 'k':
        // Define chipset
        define_chipset(optarg);
        break;

    case 'p':
        // Serial port
        strcpy(devname, optarg);
        break;

    case 'e':
        // Disable ECC
        eccflag = 1;
        break;

    case 'o':
        // Output file name
        strcpy(filename, optarg);
        break;

    case 'b':
        // Starting block number
        sscanf(optarg, "%x", &start);
        break;

    case 'l':
        // Number of blocks to read
        sscanf(optarg, "%x", &len);
        break;

    case 'z':
        // OOB size per page
        sscanf(optarg, "%u", &forced_oobsize);
        break;

    case 'u':
        if (bad_processing_flag != BAD_UNDEF) {
            printf("\n Duplicate -u option - error\n");
            return;
        }
        switch (*optarg) {
        case 'f':
            bad_processing_flag = BAD_FILL;
            break;
        case 'i':
            bad_processing_flag = BAD_IGNORE;
            break;
        case 's':
            bad_processing_flag = BAD_SKIP;
            break;
        case 'x':
            bad_processing_flag = BAD_DISABLE;
            break;
        default:
            printf("\n Invalid value for -u option\n");
            return;
        }
        break;

    case 'r':
        switch (*optarg) {
        case 'a':
            rflag = RF_AUTO; // automatic
            break;
        case 's':
            rflag = RF_STANDARD; // standard
            break;
        case 'l':
            rflag = RF_LINUX; // Linux
            break;
        case 'y':
            rflag = RF_YAFFS;
            break;
        default:
            printf("\n Invalid value for -r option\n");
            return;
        }
        break;

    case 'x':
        // Read full sector
        xflag = 1;
        break;

    case 's':
        // Read partition map
        partflag = 1;
        strcpy(ptable_file, optarg);
        break;

    case 'm':
        // List partitions mode
        partflag = 1;
        listmode = 1;
        break;

    case 'f':
        // Read specific partition(s)
        partflag = 1;
        if (optarg[0] == '*') {
            // All partitions
            partnumber = -1;
            break;
        }
        // Building the list of partitions
        partnumber = 1;
        sscanf(optarg, "%u", &i);
        partlist[i] = 1;
        break;

    case 't':
        // Trim FF bytes
        truncflag = 1;
        break;

    case 'q':
        // Disable error reading list output
        qflag = 1;
        break;

    case '?':
    case ':':
        return;
    }
}

// Check for execution without any options
if ((start == 0) && (len == 0) && !xflag && !partflag) {
  printf("\n No operation mode options specified\n");
  return;
}

// Determine the default value for the -u option
if (bad_processing_flag == BAD_UNDEF) {
  if (partflag == 0)
    bad_processing_flag = BAD_FILL; // for block range reading
  else
    bad_processing_flag = BAD_SKIP; // for reading partitions
}

// Check for compatibility of -t and -x options
if (truncflag == 1 && xflag == 1) {
  printf("\nOptions -t and -x are not compatible\n");
  return;
}

// Configure serial port and flash parameters
// In the mode of outputting the partition map from a file, skip all this configuration
//----------------------------------------------------------------------------
if (!(listmode && ptable_file[0] != '@')) {
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

    // Load flash parameters
    hello(0);

    // Allocate memory for the block buffer
    blockbuf = (unsigned char *)malloc(300000);

    if (forced_oobsize != -1) {
        printf("\n! Using OOB size %d instead of %d\n", forced_oobsize, oobsize);
        oobsize = forced_oobsize;
    }

    cwsize = sectorsize;
    if (xflag)
        cwsize += oobsize / spp; // Increase codeword size for OOB portion for each sector

    mempoke(nand_ecc_cfg, mempeek(nand_ecc_cfg) & 0xfffffffe | eccflag); // Enable/disable ECC
    mempoke(nand_cfg1, mempeek(nand_cfg1) & 0xfffffffe | eccflag); // Enable/disable ECC

    mempoke(nand_cmd, 1); // Reset all controller operations
    mempoke(nand_exec, 0x1);
    nandwait();

    // Set command code
    mempoke(nand_cmd, 0x34); // Read data+ecc+spare

    // Clear the sector buffer
    for (i = 0; i < cwsize; i += 4)
        mempoke(sector_buf + i, 0xffffffff);
}

//###################################################
// Raw Flash Reading Mode
//###################################################

if (partflag == 0) {
    if (len == 0)
        len = maxblock - start; // Read until the end of the flash
    if (filename[0] == 0) {
        switch (rflag) {
        case RF_AUTO:
        case RF_STANDARD:
            strcpy(filename, "qrflash.bin");
            break;
        case RF_LINUX:
            strcpy(filename, "qrflash.oob");
            break;
        case RF_YAFFS:
            strcpy(filename, "qrflash.yaffs");
            break;
        }
    }
    out = fopen(filename, "wb");
    if (out == 0) {
        printf("\n Error opening the output file %s", filename);
        return;
    }
    read_raw(start, len, cwsize, out, rflag);
    fclose(out);
    return;
}

//###################################################
// Partition Reading Mode
//###################################################

// Load partition table
if (!load_ptable(ptable_file)) {
    printf("\n Partition table not found. Exiting.\n");
    return;
}

// Check if the partition table loaded successfully
if (!validpart) {
    printf("\nPartition table not found or corrupted\n");
    return;
}

// Partition list mode
if (listmode) {
    list_ptable();
    return;
}

if ((partnumber != -1) && (partnumber >= fptable.numparts)) {
    printf("\nInvalid partition number: %i, total partitions %u\n", partnumber, fptable.numparts);
    return;
}

print_ptable_head();

// Main loop - iterate over partitions
for (i = 0; i < fptable.numparts; i++) {
    // Read the partition

    //  If not in the mode of all partitions, check if this partition is selected
    if ((partnumber == -1) || (partlist[i] == 1)) {
        // Output partition description
        show_part(i);

        // Generate a filename depending on the format
        if (rflag == RF_YAFFS)
            sprintf(filename, "%02u-%s.yaffs2", i, part_name(i));
        else if (cwsize == sectorsize)
            sprintf(filename, "%02u-%s.bin", i, part_name(i));
        else
            sprintf(filename, "%02u-%s.oob", i, part_name(i));

        // Replace colons with hyphens in the filename
        if (filename[4] == ':')
            filename[4] = '-';

        // Open the output file
        out = fopen(filename, "wb");
        if (out == 0) {
            printf("\n Error opening the output file %s\n", filename);
            return;
        }

        // Loop over blocks in the partition
        for (block = part_start(i); block < (part_start(i) + part_len(i)); block++) {
            printf("\r * Reading: Block %06x [start+%03x] (%i%%)", block, block - part_start(i),
                   (block - part_start(i) + 1) * 100 / part_len(i));
            fflush(stdout);

            // Read the block
            switch (rflag) {
            case RF_AUTO: // automatic format selection
                if ((fptable.part[i].attr2 != 1) || (cwsize > (sectorsize + 4)))
                    // raw reading or reading of unaltered partitions
                    badflag = read_block(block, cwsize, out);
                else
                    // reading of Chinese Linux partitions
                    badflag = read_block_resequence(block, out);
                break;

            case RF_STANDARD: // standard format
                badflag = read_block(block, cwsize, out);
                break;

            case RF_LINUX: // Chinese Linux format
                badflag = read_block_resequence(block, out);
                break;

            case RF_YAFFS: // file partition image
                badflag = read_block_yaffs(block, out);
                break;
            }

            // Show a list of errors found
            show_errlist();
            if (badflag != 0) {
                printf(" - Bad block");
                if (bad_processing_flag == BAD_SKIP)
                    printf(", skipping");
                if (bad_processing_flag == BAD_IGNORE)
                    printf(", reading as is");
                if (bad_processing_flag == BAD_FILL)
                    printf(", marking in the output file");
                printf("\n");
            }
        } // End of block loop

        fclose(out);

        // Trim all trailing FF bytes
        if (truncflag) {
            out = fopen(filename, "r+b"); // Reopen the output file
            fseek(out, 0, SEEK_SET); // Rewind the file to the beginning
            lastpos = 0;
            for (filepos = 0;; filepos++) {
                c = fgetc(out);
                if (feof(out))
                    break;
                if (c != 0xff)
                    lastpos = filepos; // Found a non-FF byte
            }
#ifndef WIN32
            ftruncate(fileno(out), lastpos + 1); // Trim the file
#else
            _chsize(_fileno(out), lastpos + 1); // Trim the file
#endif
            fclose(out);
        }
    } // Check partition selection
} // End of partition loop

// Restore ECC settings
mempoke(nand_ecc_cfg, mempeek(nand_ecc_cfg) & 0xfffffffe);
mempoke(nand_cfg1, mempeek(nand_cfg1) & 0xfffffffe);

printf("\n");

}
