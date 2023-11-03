//
// Flash Memory Partition Table Procedures
//
#include "include.h"

// Storage for the flash partition table
struct flash_partition_table fptable;
int validpart = 0; // Validity of the partition table

//*************************************
// Load the partition table from flash
//*************************************
int load_ptable_flash() {
    unsigned int udsize = 512;
    unsigned int blk;
    unsigned char buf[4096];

    if (get_udflag()) udsize = 516;

    for (blk = 0; blk < 12; blk++) {
        // Look for a block with maps
        flash_read(blk, 0, 0);  // Page 0, Sector 0 - MIBIB block header
        memread(buf, sector_buf, udsize);

        // Check the MIBIB header signature
        if (memcmp(buf, "\xac\x9f\x56\xfe\x7a\x12\x7f\xcd", 8) != 0) continue;  // Signature not found, continue searching

        // Found a MIBIB block - Page 1 contains the system partition table
        // Load this page into our buffer
        flash_read(blk, 1, 0);  // Page 1, Sector 0 - System partition table
        memread(buf, sector_buf, udsize);
        mempoke(nand_exec, 1);  // Sector 1 - continuation of the table
        nandwait();
        memread(buf + udsize, sector_buf, udsize);

        // Copy the partition table image into the structure
        memcpy(&fptable, buf, sizeof(fptable));

        // Check the signature of the system table
        if ((fptable.magic1 != FLASH_PART_MAGIC1) || (fptable.magic2 != FLASH_PART_MAGIC2)) continue;

        // Found the table
        validpart = 1;

        // Adjust the length of the last partition if maxblock is not zero
        if ((maxblock != 0) && (fptable.part[fptable.numparts - 1].len == 0xffffffff)) {
            fptable.part[fptable.numparts - 1].len = maxblock - fptable.part[fptable.numparts - 1].offset; // If the length is FFFF, it represents a growing partition.
        }

		return 1;  // The table is found; there's nothing more to do here
    }

    validpart = 0;
    return 0;
}

//***************************************************
//* Load Partition Table from an External File
//*
//* If the filename consists of a single hyphen '-' -
//* load from the file ptable/current-r.bin
//***************************************************
int load_ptable_file(char* name) {
    char filename[200];
    unsigned char buf[4096];
    FILE* pf;

    if (name[0] == '-') {
        strcpy(filename, "ptable/current-r.bin");
    } else {
        strncpy(filename, name, 199);
    }

    pf = fopen(filename, "rb");
    if (pf == 0) {
        printf("\n! Error opening partition table file %s\n", filename);
        return 0;
    }

    fread(buf, 1024, 1, pf);  // Read the partition table from the file
    fclose(pf);

    // Copy the partition table image into the structure
    memcpy(&fptable, buf, sizeof(fptable));
    validpart = 1;
    return 1;
}

//*****************************************************
//* Universal Procedure for Loading Partition Tables
//*
//* Possible options for 'name':
//*
//*    @ - Load the table from flash memory
//*    - - Load from the file ptable/current-r.bin
//*  name - Load from the specified file
//*
//*****************************************************
int load_ptable(char* name) {
    if (name[0] == '@') {
        return load_ptable_flash();
    } else {
        return load_ptable_file(name);
    }
}

//***************************************************
// Print the header of the partition table
//***************************************************
void print_ptable_head() {
    printf("\n #  Start  Length  A0 A1 A2 F#  Format ------ Name------");
    printf("\n============================================================\n");
}

//***************************************************
// Print information about a partition by its number
//***************************************************
int show_part(int pn) {
    if (!validpart) return 0;  // Table has not been loaded yet
    if (pn >= fptable.numparts) return 0;  // Invalid partition number

    printf("\r%02u  %6x", pn, fptable.part[pn].offset);

    if (fptable.part[pn].len != 0xffffffff) {
        printf("  %6.6x   ", fptable.part[pn].len);
    } else {
        printf("  ------   ");
    }

    printf("%02x %02x %02x %02x   %s   %.16s\n",
           fptable.part[pn].attr1,
           fptable.part[pn].attr2,
           fptable.part[pn].attr3,
           fptable.part[pn].which_flash,
           (fptable.part[pn].attr2 == 1) ? "LNX" : "STD",
           fptable.part[pn].name);

    return 1;
}

//*************************************
// List the partition table on the screen
//*************************************
void list_ptable() {
    int i;

    if (!validpart) return;  // The table has not been loaded yet
    print_ptable_head();
    for (i = 0; i < fptable.numparts; i++) show_part(i);
    printf("============================================================");
    printf("\n Partition table version: %i\n", fptable.version);
}

//*************************************
// Get the name of a partition by its number
//*************************************
char* part_name(int pn) {
    return fptable.part[pn].name;
}

//*********************************************
// Get the starting block of a partition by its number
//*********************************************
int part_start(int pn) {
    return fptable.part[pn].offset;
}

//*********************************************
// Get the length of a partition by its number
//*********************************************
int part_len(int pn) {
    return fptable.part[pn].len;
}

//************************************************************
// Get the partition number that a given block belongs to
//************************************************************
int block_to_part(int block) {
    int i;
    for (i = 0; i < fptable.numparts; i++) {
        if ((block >= part_start(i)) && (block < (part_start(i) + part_len(i)))) {
            return i;
        }
    }
    return -1;  // Partition not found
}
