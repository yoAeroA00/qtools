#include "include.h"

void main(int argc, char* argv[]) {
  
    FILE* in;
    unsigned int fl, i, j, daddr, ncmd, tablefound = 0, hwidfound = 0;
    unsigned char buf[1024 * 512];
    unsigned char hwidstr[7] = "HW_ID1";
    unsigned char hwid[17];
    unsigned int header[7];
    unsigned int baseaddr = 0;
    unsigned int codeoffset = 0x28;

    if (argv[1] == NULL) {
        printf("\nFile not specified\n");
        return;
    }
    in = fopen(argv[1], "rb");
    if (in == NULL) {
        printf("\nCannot open file %s\n", argv[1]);
        return;
    }

    fl = fread(&buf, 1, sizeof(buf), in);
    fclose(in);
    printf("\n** %s: %u bytes\n", argv[1], fl);

    // Analyze the bootloader header
    memcpy(header, buf, 28);
    if (header[1] == 0x73d71034) {
        // New ENPRG(Bootloader) header format
        baseaddr = header[6] - header[5];
        codeoffset = 0x50;
    }
    else if (header[1] == 3) {
        // Format for NPRG and old ENPRG
        baseaddr = header[3] - 0x28;
    }
    else printf("\nUndefined file header - likely not a bootloader");
    if (baseaddr != 0) { 
        printf("\nLoad Address:    %08x", baseaddr);
        printf("\nCode Start Address: %08x", baseaddr + codeoffset);
    }

    for (i = 0; i < fl; i++) {
        if (hwidfound == 0) {
            if (((memcmp(buf + i, hwidstr, 6)) == 0) && (buf[i - 17] == 0x30)) { // Search for "HW_ID" in the certificate
                memcpy(hwid, buf + i - 17, 16); // Found, save the value
                hwid[16] = 0;
                hwidfound = 1; // Don't search further
            }
        }
        if ((i >= 0x1000) && (i % 4 == 0) && (tablefound == 0)) { // Table is word-aligned and not processed yet
            daddr = *((unsigned int*)&buf[i]); // Handler address
            if ((daddr < 0x10000) || (daddr > 0xffff0000)) continue;
            if ( (daddr != *((unsigned int*)&buf[i - 4])) && 
                (daddr == *((unsigned int*)&buf[i + 8])) && 
                (daddr == *((unsigned int*)&buf[i + 16])) && 
                (daddr == *((unsigned int*)&buf[i + 24])) && 
                (daddr == *((unsigned int*)&buf[i + 32])) && 
                (daddr == *((unsigned int*)&buf[i + 40])) &&
                (daddr == *((unsigned int*)&buf[i + 48])) &&
                (daddr == *((unsigned int*)&buf[i + 56])) && 
                (daddr == *((unsigned int*)&buf[i + 64])) &&
                (daddr == *((unsigned int*)&buf[i + 72])) &&
                (daddr == *((unsigned int*)&buf[i + 80])) &&
                (daddr == *((unsigned int*)&buf[i + 88]))
            ) {
                // Table found
                tablefound = 1;
                // Loop to extract commands from the table
                ncmd = 0;
                for (j = 0; j < 0x100; j++) {
                    if (*((unsigned int*)&buf[i - 4 + 4 * j]) != daddr) {
                        if ((*((unsigned int*)&buf[i - 4 + 4 * j]) & 0xfffc0000) != (daddr & 0xfffc0000)) break; // Not an address, end of the table
                        ncmd++;
                        printf("\nCMD %02x = %08x", j + 1, *((unsigned int*)&buf[i - 4 + 4 * j]));
                    }
                }  
                if (ncmd != 0) {
                    printf("\nCMD Table Offset: %x", i - 4);
                    printf("\nInvalid CMD Handler: %08x", daddr);
                } else  printf("\nNo CMD table found. Image is probably not a *PRG.");  
            }
        }
    }
    if (hwidfound == 1) {
        printf("\n\nHW_ID = 0x%.16s", hwid);
        printf("\nMSM_ID = 0x%.8s\nOEM_ID = 0x%.4s\nMODEL_ID = 0x%.4s\n", hwid, hwid + 8, hwid + 12);
    } else printf("\n\nUnsigned code or no HW_ID value in Subject field\n");

    printf("\n");
}
