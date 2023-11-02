//
// Procedures for working with the modem's memory through the bootloader
//
#include "include.h"

//***********************
//* Memory area dump  *
//***********************

void dump(unsigned char buffer[], unsigned int len, unsigned int base) {
    unsigned int i, j;
    char ch;

    for (i = 0; i < len; i += 16) {
        printf("%08x: ", base + i);
        for (j = 0; j < 16; j++) {
            if ((i + j) < len)
                printf("%02x ", buffer[i + j] & 0xff);
            else
                printf("   ");
        }
        printf(" *");
        for (j = 0; j < 16; j++) {
            if ((i + j) < len) {
                ch = buffer[i + j];
                if ((ch < 0x20) || (ch > 0x80))
                    ch = '.';
                putchar(ch);
            } else
                putchar(' ');
        }
        printf("*\n");
    }
}

//***********************************8
//* Read a memory area
//***********************************8

int memread(unsigned char* membuf, int adr, int len) {
    char iobuf[11600];
    int tries;   // number of command retry attempts
    int errcount = 0; // error counter

    // Reading parameters - offsets:
    const int adroffset = 0x2E; // address offset
    const int lenoffset = 0x32; // data length

    char cmdbuf[] = {
        0x11, 0x00, 0x24, 0x30, 0x9f, 0xe5, 0x24, 0x40, 0x9f, 0xe5, 0x12, 0x00, 0xa0, 0xe3, 0x04, 0x00,
        0x81, 0xe4, 0x04, 0x00, 0x83, 0xe0, 0x04, 0x20, 0x93, 0xe4, 0x04, 0x20, 0x81, 0xe4, 0x00, 0x00,
        0x53, 0xe1, 0xfb, 0xff, 0xff, 0x3a, 0x04, 0x40, 0x84, 0xe2, 0x1e, 0xff, 0x2f, 0xe1, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    int i, iolen;
    // Initial fragment length
    int blklen = 1000;
    *((unsigned int*)&cmdbuf[lenoffset]) = blklen;

    ///////////////////
    //printf("\n!ms adr=%08x len=%08x",adr,len);

    // Reading blocks in 1000-byte chunks
    for (i = 0; i < len; i += 1000) {
        tries = 20; // number of block data read attempts
        *((unsigned int*)&cmdbuf[adroffset]) = i + adr; // set the address
        if ((i + 1000) > len) {
            // Last block of data may be short
            blklen = len - i;
            *((unsigned int*)&cmdbuf[lenoffset]) = blklen; // set the length
        }

        // Make several attempts to send the command and read the data
        while (tries > 0) {
            iolen = send_cmd_massdata(cmdbuf, sizeof(cmdbuf), iobuf, blklen + 4);
            if (iolen < (blklen + 4)) {
                // Short response from the bootloader
                tries--;
                usleep(1000);
				//printf("\n!t%i! %i < %i",tries,iolen,blklen+4);
            } else
                break; // Normal response - finish with this data block
        }
        if (tries == 0) {
            printf("\n Memory read command processing error, %i bytes required, received %i at address %08x\n", blklen, iolen, adr);
            memset(membuf + i, 0xEB, blklen);
            errcount++;
        } else
            memcpy(membuf + i, iobuf + 5, blklen);
    }
    return !errcount;
}

//***********************************8
//* Read a word from memory
//***********************************8
unsigned int mempeek(int adr) {
    unsigned int data;
    memread((unsigned char*)&data, adr, 4);
    return data;
}

//******************************************
//* Write an array to memory
//******************************************
int memwrite(unsigned int adr, unsigned char* buf, unsigned int len) {
    // Writing parameters - offsets:
    const int adroffset = 0x32; // address offset
    const int lenoffset = 0x36; // data length
    const int dataoffset = 0x3A; // data start

    char cmdbuf[1028] = {
        0x11, 0x00, 0x38, 0x00, 0x80, 0xe2, 0x24, 0x30, 0x9f, 0xe5, 0x24, 0x40, 0x9f, 0xe5, 0x04, 0x40,
        0x83, 0xe0, 0x04, 0x20, 0x90, 0xe4, 0x04, 0x20, 0x83, 0xe4, 0x04, 0x00, 0x53, 0xe1, 0xfb, 0xff,
        0xff, 0x3a, 0x12, 0x00, 0xa0, 0xe3, 0x00, 0x00, 0xc1, 0xe5, 0x01, 0x40, 0xa0, 0xe3, 0x1e, 0xff,
        0x2f, 0xe1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    unsigned char iobuf[1024];

    if (len > 1000)
        exit(1);  // Limit the buffer size
    memcpy(cmdbuf + dataoffset, buf, len);
    *((unsigned int*)&cmdbuf[adroffset]) = adr;
    *((unsigned int*)&cmdbuf[lenoffset]) = len;
    send_cmd(cmdbuf, len + dataoffset, iobuf);

    return 1;
}

//******************************************
//* Write a word to memory
//******************************************
int mempoke(int adr, int data) {
    unsigned int data1 = data;
    return (memwrite(adr, (unsigned char*)&data1, 4));
}
