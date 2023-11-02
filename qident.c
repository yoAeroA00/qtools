#include "include.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Identification of device chipsets (with DMSS protocol)
//

void main(int argc, char* argv[]) {

    unsigned char iobuf[2048];
    unsigned int iolen;

    char id_cmd[2] = {0, 0};
    char flashid_cmd[4] = {0x4b, 0x13, 0x15, 0};

    unsigned short chip_code;
    int chip_id;
    int opt;

    struct {
        int32 hdr;
        int32 diag_errno;            /* Error code if error, 0 otherwise  */
        int32 total_no_of_blocks;    /* Total number of blocks in the device */
        int32 no_of_pages_per_block; /* Number of pages in a block */
        int32 page_size;             /* Size of page data region in bytes */
        int32 total_page_size;       /* Size of total page_size */
        int32 maker_id;              /* Device maker ID */
        int32 device_id;             /* Device ID */
        uint8 device_type;           /* 0 indicates NOR device 1 NAND */
        uint8 device_name[15];       /* Device name */
    } fid;

#ifndef WIN32
    char devname[20] = "/dev/ttyUSB0";
#else
    char devname[20] = "";
#endif

    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
        case 'h':
            printf("\nThis utility is designed to identify chipsets of devices that support the DMSS protocol.\n\
The utility works with the diagnostic port of a device in normal (working) mode.\n\n\
The following options are available:\n\n\
-p <tty>  - Specifies the name of the serial port device for communication with the bootloader\n\
");
            return;

        case 'p':
            strcpy(devname, optarg);
            break;

        case '?':
        case ':':
            return;
        }
    }

#ifdef WIN32
    if (*devname == '\0')
    {
        printf("\n - Serial port is not specified\n");
        return;
    }
#endif

    if (!open_port(devname))  {
#ifndef WIN32
        printf("\n - Serial port %s cannot be opened\n", devname);
#else
        printf("\n - Serial port COM%s cannot be opened\n", devname);
#endif
        return;
    }

    iolen = send_cmd_base(id_cmd, 1, iobuf, 0);
    if (iolen == 0) {
        printf("\nNo response to the identification command\n");
        return;
    }

    if (iobuf[0] != 0) {
        printf("\nIncorrect response to the identification command\n");
        return;
    }
    // Extract the chip code
    chip_code = *((unsigned short*)&iobuf[0x35]);
    // Reverse the chip id
    chip_code = (((chip_code & 0xff) << 8) | ((chip_code & 0xff00) >> 8));
    // Get the chipset ID based on the code
    chip_id = find_chipset(chip_code);
    if (chip_id == -1) {	
        printf("\nChipset not recognized, chipset code - %04x\n", chip_code);
        return;
    }
    // Chipset found
    set_chipset(chip_id);
    printf("\nChipset: %s  (%08x)\n", get_chipname(), nand_cmd); fflush(stdout);

    // Determine the type and parameters of the flash memory
    iolen = send_cmd_base(flashid_cmd, 4, iobuf, 0);
    if (iolen == 0) {
        printf("\nFlash identification error\n");
        return;
    }

    memcpy(&fid, iobuf, sizeof(fid));
    if (fid.diag_errno != 0) {
        printf("\nFlash identification error\n");
        return;
    }
    printf("\nFlash: %s %s, vid=%02x  pid=%02x", (fid.device_type ? "NAND" : "NOR"), fid.device_name, fid.maker_id, fid.device_id);
    //printf("\nEFS Size:      %i blocks", fid.total_no_of_blocks);
    printf("\nPages per block: %i", fid.no_of_pages_per_block);
    printf("\nPage size:      %i", fid.page_size);
    printf("\nOOB size:       %i", fid.total_page_size - fid.page_size);
    printf("\n");
}
