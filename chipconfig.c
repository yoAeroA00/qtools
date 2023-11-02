//
// Procedures for working with chipset configuration
//
#include "include.h"

// Global variables - we collect them here

// Chipset type:
int chip_type = -1; // Current chipset index in the chipset table
int maxchip = -1;   // Number of chipsets in the configuration

// Chipset descriptors
struct {
  unsigned int id;         // Chipset code (id)
  unsigned int nandbase;   // Controller address
  unsigned char udflag;    // udsize of the partition table, 0-512, 1-516
  char name[20];  // Chipset name
  unsigned int ctrl_type;  // NAND controller register layout
  unsigned int sahara;     // Sahara protocol flag
  char nprg[40];           // Name of the nprg bootloader
  char enprg[40];         // Name of the enprg bootloader
} chipset[100];

// Chipset identification code table, up to 20 codes per chipset
unsigned short chip_code[100][20];

// Controller register addresses (offsets relative to the base)
struct {
  unsigned int nand_cmd;
  unsigned int nand_addr0;
  unsigned int nand_addr1;
  unsigned int nand_cs;
  unsigned int nand_exec;
  unsigned int nand_buffer_status;
  unsigned int nand_status;
  unsigned int nand_cfg0;
  unsigned int nand_cfg1;
  unsigned int nand_ecc_cfg;
  unsigned int NAND_FLASH_READ_ID;
  unsigned int sector_buf;
} nandreg[] = {
  // cmd  adr0   adr1    cs    exec  buf_st fl_st  cfg0   cfg1   ecc     id   sbuf
  {0, 4, 8, 0xc, 0x10, 0x18, 0x14, 0x20, 0x24, 0x28, 0x40, 0x100}, // ctrl=0 - new
  {0x304, 0x300, 0xffff, 0x30c, 0xffff, 0xffff, 0x308, 0xffff, 0x328, 0xffff, 0x320, 0}  // ctrl=1 - old
};

// Controller commands
struct {
  unsigned int stop;      // Stop controller operations
  unsigned int read;      // Data only
  unsigned int readall;   // Data + ECC + spare
  unsigned int program;    // Data only
  unsigned int programall; // Data + ECC + spare
  unsigned int erase;
  unsigned int identify;
} nandopcodes[] = {
  //  stop   read  readall  program  programall erase  identify
  {0x01, 0x32, 0x34, 0x36, 0x39, 0x3a, 0x0b},   // ctrl=0 - new
  {0x07, 0x01, 0xffff, 0x03, 0xffff, 0x04, 0x05}    // ctrl=1 - old
};

// Global controller register addresses

unsigned int nand_addr0;
unsigned int nand_addr1;
unsigned int nand_cs;
unsigned int nand_exec;
unsigned int nand_status;
unsigned int nand_buffer_status;
unsigned int nand_cfg0;
unsigned int nand_cfg1;
unsigned int nand_ecc_cfg;
unsigned int NAND_FLASH_READ_ID;
unsigned int sector_buf;

// Global command code storage

unsigned int nc_stop, nc_read, nc_readall, nc_program, nc_programall, nc_erase, nc_identify;

//************************************************
//*   Load chipset configuration
//************************************************
int load_config() {
  char line[300];
  char* tok1, *tok2;
  int index;
  int msmidcount;

  char vname[50];
  char vval[100];

  FILE* in = qopenfile("chipset.cfg", "r");
  if (in == 0) {
    printf("\n! Chipset configuration file chipset.cfg not found\n");
    return 0;  // Configuration not found
  }

  while (fgets(line, 300, in) != 0) {
    if (strlen(line) < 3) continue; // Too short line
    if (line[0] == '#') continue; // Comment
    index = strspn(line, " ");  // Get a pointer to the start of the informative part of the line
    tok1 = line + index;

    if (strlen(tok1) == 0) continue; // Line consists of spaces

    // Start of the descriptor of the current chipset
    if (tok1[0] == '[') {
      //printf("\n@@ описатель чипсета:");
      tok2 = strchr(tok1, ']');
      if (tok2 == 0) {
        printf("\n! The configuration file contains an error in the chipset header\n %s\n", line);
        return 0;
      }
      tok2[0] = 0;
      // Start creating the descriptor structure of the current chipset
      maxchip++; // Index in the structure
      chipset[maxchip].id = 0;         // Chipset code (id) 0 - non-existent chipset
      chipset[maxchip].nandbase = 0;   // Controller address
      chipset[maxchip].udflag = 0;    // udsize of the partition table, 0-512, 1-516
      strcpy(chipset[maxchip].name, tok1 + 1);  // Chipset name
      chipset[maxchip].ctrl_type = 0;  // NAND controller register layout
      chipset[maxchip].nprg[0] = 0;
      chipset[maxchip].enprg[0] = 0;
      memset(chip_code[maxchip], 0xffff, 40);  // Fill the msm_id table with FF
      msmidcount = 0;
      //printf("\n@@ %s",tok1+1);
      continue;
    }

    if (maxchip == -1) {
      printf("\n! The configuration file contains lines outside the chipset description section\n");
      return 0;
    }

    // The line is one of the variables describing the chipset
    memset(vname, 0, sizeof(vname));
    memset(vval, 0, sizeof(vval));
    // Extract the variable descriptor
    index = strspn(line, " ");  // Get a pointer to the start of the informative part of the line
    tok1 = line + index; // Start of the token
    index = strcspn(tok1, " ="); // End of the token
    memcpy(vname, tok1, index); // Get the variable name

    tok1 += index;
    if (strchr(tok1, '=') == 0) {
      printf("\n! Configuration file: no variable value\n%s\n", line);
      return 0;
    }
    tok1 += strspn(tok1, "= "); // Skip the separator
    strncpy(vval, tok1, strcspn(tok1, " \r\n"));

    //printf("\n @@@ vname = <%s>   vval = <%s>",vname,vval);

    // Parse variable names

    // Chipset id
    if (strcmp(vname, "id") == 0) {
      chipset[maxchip].id = atoi(vval);         // Chipset code (id) 0 - non-existent chipset
      if (chipset[maxchip].id == 0) {
        printf("\n! Configuration file: id=0 is not allowed\n%s\n", line);
        return 0;
      }
      continue;
    }

    // Controller address
    if (strcmp(vname, "addr") == 0) {
      sscanf(vval, "%x", &chipset[maxchip].nandbase);
      continue;
    }

    // udflag
    if (strcmp(vname, "udflag") == 0) {
      chipset[maxchip].udflag = atoi(vval);
      continue;
    }

    // Controller type
    if (strcmp(vname, "ctrl") == 0) {
      chipset[maxchip].ctrl_type = atoi(vval);
      continue;
    }

    // msm_id table
    if (strcmp(vname, "msmid") == 0) {
      sscanf(vval, "%hx", &chip_code[maxchip][msmidcount++]);
      continue;
    }

    // Sahara protocol flag
    if (strcmp(vname, "sahara") == 0) {
      chipset[maxchip].sahara = atoi(vval);
      continue;
    }

    // Default NPRG bootloader name
    if (strcmp(vname, "nprg") == 0) {
      strncpy(chipset[maxchip].nprg, vval, 39);
      continue;
    }

    // Default ENPRG bootloader name
    if (strcmp(vname, "enprg") == 0) {
      strncpy(chipset[maxchip].enprg, vval, 39);
      continue;
    }

    // Other variable names
    printf("\n! Configuration file: invalid variable name\n%s\n", line);
    return 0;
  }
  fclose(in);
  if (maxchip == -1) {
    printf("\n! The configuration file does not contain any chipset descriptors\n");
    return 0;
  }
  maxchip++;
  return 1;
}

//************************************************
//*   Find a chipset by msm_id
//************************************************
int find_chipset(unsigned short code) {
  int i, j;
  if (maxchip == -1)
    if (!load_config()) exit(0); // Configuration did not load
  for (i = 0; i < maxchip; i++) {
    for (j = 0; j < 20; j++) {
      if (code == chip_code[i][j]) return chipset[i].id;
      if (chip_code[i][j] == 0xffff) break;
    }
  }
  // Not found
  return -1;
}

//************************************************
//* Print the list of supported chipsets
//************************************************
void list_chipset() {

  int i, j;
  printf("\n Code     Name    NAND Address   Type  udflag  MSM_ID\n---------------------------------------------------------------------");
  for (i = 0; i < maxchip; i++) {
    //  if (i == 0)  printf("\n  0 (default) chipset auto-detection");
    printf("\n %2i  %9.9s    %08x    %1i     %1i    ", chipset[i].id, chipset[i].name,
      chipset[i].nandbase, chipset[i].ctrl_type, chipset[i].udflag);
    for (j = 0; chip_code[i][j] != 0xffff; j++)
      printf(" %04hx", chip_code[i][j]);
  }
  printf("\n\n");
  exit(0);
}

//*******************************************************************************
//* Set the chipset type based on the chipset code from the command line
//* 
//* arg - pointer to optarg specified with the -k option
//****************************************************************
void define_chipset(char* arg) {

  unsigned int c;

  if (maxchip == -1)
    if (!load_config()) exit(0); // Configuration did not load
  // Check for -kl
  if (optarg[0] == 'l') list_chipset();

  // Get the chipset code from the argument
  sscanf(arg, "%u", &c);
  set_chipset(c);
}

//****************************************************************
//* Set controller parameters based on the chipset type
//****************************************************************
void set_chipset(unsigned int c)
{
    if(maxchip == -1 && !load_config())
        exit(0); // Configuration did not load

    // Get the size of the chipset array
    for(int i = 0; i < maxchip ;i++)
        // Check our number
        if(chipset[i].id == c)
            chip_type = i;

    if (chip_type < 0)
    {
        printf("\n - Invalid chipset code - %d\n", c);
        exit(1);
    }

    // Set controller register addresses
    #define setnandreg(name) name = chipset[chip_type].nandbase + nandreg[chipset[chip_type].ctrl_type].name;
    setnandreg(nand_cmd)
    setnandreg(nand_addr0)
    setnandreg(nand_addr1)
    setnandreg(nand_cs)
    setnandreg(nand_exec)
    setnandreg(nand_status)
    setnandreg(nand_buffer_status)
    setnandreg(nand_cfg0)
    setnandreg(nand_cfg1)
    setnandreg(nand_ecc_cfg)
    setnandreg(NAND_FLASH_READ_ID)
    setnandreg(sector_buf)
}

//**************************************************************
//* Get the name of the current chipset
//**************************************************************
unsigned char* get_chipname() {
  return chipset[chip_type].name;
}

//**************************************************************
//* Get the NAND controller type
//**************************************************************
unsigned int get_controller() {
  return chipset[chip_type].ctrl_type;
}

//**************************************************************
//* Get the Sahara protocol flag
//**************************************************************
unsigned int get_sahara() {
  return chipset[chip_type].sahara;
}

//************************************************************
//* Check the chipset name
//************************************************************
int is_chipset(char* name) {
  if (strcmp(chipset[chip_type].name, name) == 0) return 1;
  return 0;
}

//**************************************************************
//* Get udsize
//**************************************************************
unsigned int get_udflag() {
  return chipset[chip_type].udflag;
}

//**************************************************************
//* Get the default NPRG bootloader name
//**************************************************************
char* get_nprg() {
  return chipset[chip_type].nprg;
}

//**************************************************************
//* Get the default ENPRG bootloader name
//**************************************************************
char* get_enprg() {
  return chipset[chip_type].enprg;
}
