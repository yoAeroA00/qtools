#ifndef __PTABLE_H__
#define __PTABLE_H__

//#######################################################################
//# Partition Table Structure Description
//#######################################################################

// Length of the partition name. It is fixed, and the name is not required to end with 0
#define FLASH_PART_NAME_LENGTH 16

// Signature of the system partition table (read table)
#define FLASH_PART_MAGIC1     0x55EE73AA
#define FLASH_PART_MAGIC2     0xE35EBDDB

// Signature of the user partition table (write table)
#define FLASH_USR_PART_MAGIC1     0xAA7D1B9A
#define FLASH_USR_PART_MAGIC2     0x1F7D48BC

// Identifier for the size of a partition that extends towards the end of the flash
#define FLASH_PARTITION_GROW  0xFFFFFFFF

// Default attribute values
#define FLASH_PART_FLAG_BASE  0xFF
#define FLASH_PART_ATTR_BASE  0xFF

/* Attributes for NAND partitions */
#define FLASH_PART_HEX_SIZE   0xFE

/* Attribute Masks */
#define FLASH_PART_ATTRIBUTE1_BMSK 0x000000FF
#define FLASH_PART_ATTRIBUTE2_BMSK 0x0000FF00
#define FLASH_PART_ATTRIBUTE3_BMSK 0x00FF0000

/* Attribute Shift Offsets */
#define FLASH_PART_ATTRIBUTE1_SHFT 0
#define FLASH_PART_ATTRIBUTE2_SHFT 8
#define FLASH_PART_ATTRIBUTE3_SHFT 16

#define FLASH_MI_BOOT_SEC_MODE_NON_TRUSTED 0x00
#define FLASH_MI_BOOT_SEC_MODE_TRUSTED     0x01

#define FLASH_PART_ATTRIB( val, attr ) (((val) & (attr##_BMSK)) >> attr##_SHFT)

// Description of Attribute 1
typedef enum {
  FLASH_PARTITION_DEFAULT_ATTRB = 0xFF,  // Default
  FLASH_PARTITION_READ_ONLY = 0,         // Read-only
} flash_partition_attrb_t;

// Description of Attribute 2
typedef enum {
  FLASH_PARTITION_DEFAULT_ATTRB2 = 0xFF,  // Default (512-byte sectors)
  FLASH_PARTITION_MAIN_AREA_ONLY = 0,    // Main area only (512-byte sectors)
  FLASH_PARTITION_MAIN_AND_SPARE_ECC,    // Linux format (516-byte sectors, some spare with ECC protection)
} flash_partition_attrb2_t;

// Description of Attribute 3
typedef enum {
  FLASH_PART_DEF_ATTRB3 = 0xFF,          // Default
  FLASH_PART_ALLOW_NAMED_UPGRD = 0,      // Named upgrade allowed
} flash_partition_attrb3_t;

//*******************************************************************
// Partition Entry in the System Partition Table (Read Table)
//*******************************************************************
struct flash_partition_entry;
typedef struct flash_partition_entry *flash_partentry_t;
typedef struct flash_partition_entry *flash_partentry_ptr_type;

struct flash_partition_entry {
  // Partition name
  char name[FLASH_PART_NAME_LENGTH];

  // Offset in blocks to the beginning of the partition
  uint32 offset;

  // Size of the partition in blocks
  uint32 len;

  // Partition attributes
  uint8 attr1;
  uint8 attr2;
  uint8 attr3;

  // Flash on which the partition resides (0 - primary, 1 - secondary)
  uint8 which_flash;
};

//*************************************************************************
// User Partition Entry in the User Partition Table (Write Table)
//*************************************************************************
struct flash_usr_partition_entry;
typedef struct flash_usr_partition_entry *flash_usr_partentry_t;
typedef struct flash_usr_partition_entry *flash_usr_partentry_ptr_type;

struct flash_usr_partition_entry {
  // Partition name
  char name[FLASH_PART_NAME_LENGTH];

  // Partition size in KB
  uint32 img_size;

  // Size of the reserved (for bad blocks) area of the partition in KB
  uint16 padding;

  // Flash on which the partition resides (0 - primary, 1 - secondary)
  uint16 which_flash;

  // Partition attributes (same as in the read table)
  uint8 reserved_flag1;
  uint8 reserved_flag2;
  uint8 reserved_flag3;

  uint8 reserved_flag4;
};

/*  Number of partition tables which will fit in 512 byte page, calculated
 *  by subtracting the partition table header size from 512 and dividing
 *  the result by the size of each entry.  There is no easy way to do this
 *  with calculations made by the compiler.
 *
 *     16 bytes for header
 *     28 bytes for each entry
 *     512 - 16 = 496
 *     496/28 = 17.71 = round down to 16 entries
 */

// Maximum number of partitions in the table
#define FLASH_NUM_PART_ENTRIES  32

//*************************************************************************
// Structure of the System Partition Table (Read Table)
//*************************************************************************
struct flash_partition_table {
  // Table signatures
  uint32 magic1;
  uint32 magic2;
  // Table version
  uint32 version;

  // Number of defined partitions
  uint32 numparts;
  // List of partitions
  struct flash_partition_entry part[FLASH_NUM_PART_ENTRIES];
  //int8 trash[112];
};

//*************************************************************************
// User Partition Table Structure
//*************************************************************************
struct flash_usr_partition_table {
  // Table signatures
  uint32 magic1;
  uint32 magic2;
  // Table version
  uint32 version;
  // Number of defined partitions
  uint32 numparts;  /* number of partition entries */
  // List of partitions
  struct flash_usr_partition_entry part[FLASH_NUM_PART_ENTRIES];
};

int load_ptable_flash();
int load_ptable_file(char* filename);
int load_ptable(char* name);
void print_ptable_head();
int show_part(int pn);
void list_ptable();
char* part_name(int pn);
int part_start(int pn);
int part_len(int pn);
int block_to_part(int block);

extern struct flash_partition_table fptable;
extern int validpart; // Validity of the partition table

#endif // __PTABLE_H__
