#include "include.h"

// Set flash controller parameters
//

void main(int argc, char* argv[]) {

#ifndef WIN32
char devname[20] = "/dev/ttyUSB0";
#else
char devname[20] = "";
#endif

// Local parameters for configuration
int lud = -1, lecc = -1, lspare = -1, lbad = -1;
int sflag = 0;
int opt;
int badloc;

while ((opt = getopt(argc, argv, "hp:k:s:u:e:d:")) != -1) {
    switch (opt) {
    case 'h':
        printf("\nThis utility is intended for setting NAND controller parameters\n\n\
The following options are available:\n\n\
-p <tty> - Specifies the name of the serial port device for communication with the bootloader\n\
-s nnn   - Set the size of the spare field on a sector\n\
-u nnn   - Set the size of the data field of a sector\n\
-e nnn   - Set the size of the ECC field on a sector\n\
-d [L]xxx- Set the marker for defective blocks to byte xxx (hex), L=U(user) or S(spare)\n\
");
        return;

    case 'p':
        strcpy(devname, optarg);
        break;

    case 'k':
        define_chipset(optarg);
        break;

    case 's':
        sscanf(optarg, "%d", &lspare);
        sflag = 1;
        break;

    case 'u':
        sscanf(optarg, "%d", &lud);
        sflag = 1;
        break;

    case 'e':
        sscanf(optarg, "%d", &lecc);
        sflag = 1;
        break;

    case 'd':
        parse_badblock_arg(optarg, &lbad, &badloc);
        sflag = 1;
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

if (!sflag) {
    hello(1);
    return;
}

hello(0);

if (lspare != -1) set_sparesize(lspare);
if (lud != -1) set_udsize(lud);
if (lecc != -1) set_eccsize(lecc);
if (lbad != -1) set_badmark_pos(lbad, badloc);
}
