#include "include.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//* Read a section of the modem's memory to the file qmem.bin
//
// qread <address> <length>
// Reading is done in blocks of 512 bytes. All numbers are in hex
//

void main(int argc, char* argv[]) {

    unsigned char iobuf[2048];
    unsigned char filename[300] = "qmem.bin";
    unsigned int i;
    unsigned int adr = 0, len = 0x200, endadr, blklen = 512, helloflag = 0, opt;
    FILE* out;

#ifndef WIN32
    char devname[20] = "/dev/ttyUSB0";
#else
    char devname[20] = "";
#endif

    while ((opt = getopt(argc, argv, "p:a:l:o:hi")) != -1) {
        switch (opt) {
        case 'h':
            printf("\nThis utility is designed to read the address space of the modem\n\n\
The following options are available:\n\n\
-i        - Runs the HELLO procedure to initialize the bootloader\n\
-p <tty>  - Specifies the name of the serial port device for communication with the bootloader\n\
-o <file> - Output file name (default is qmem.bin)\n\n\
-a <adr>  - Starting address\n\
-l <num>  - Size of the read section\n");
            return;

        case 'p':
            strcpy(devname, optarg);
            break;

        case 'o':
            strcpy(filename, optarg);
            break;

        case 'a':
            sscanf(optarg, "%x", &adr);
            break;

        case 'l':
            sscanf(optarg, "%x", &len);
            break;

        case 'i':
            helloflag = 1;
            break;

        case '?':
        case ':':
            return;
        }
    }

#ifdef WIN32
    if (*devname == '\0')
    {
        printf("\n - Serial port not specified\n");
        return;
    }
#endif

    if (len == 0) {
        printf("\nInvalid length");
        return;
    }

    if (!open_port(devname))  {
#ifndef WIN32
        printf("\n - Serial port %s cannot be opened\n", devname);
#else
        printf("\n - Serial port COM%s cannot be opened\n", devname);
#endif
        return;
    }

    out = fopen(filename, "wb");

    if (helloflag) hello(2);

    endadr = adr + len;
    printf("\nReading the area %08x - %08x\n", adr, endadr - 1);

    for (i = adr; i < endadr; i += 512)  {
        printf("\r %08x", i);
        if ((i + 512) > endadr) {
            blklen = endadr - adr;
        }
        memread(iobuf, i, blklen);
        fwrite(iobuf, 1, blklen, out);
    }
    printf("\n");
    fclose(out);
}
