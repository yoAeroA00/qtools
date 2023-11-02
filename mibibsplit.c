#include "include.h"

void main(int argc, char* argv[]) {

    FILE* in;
    FILE* out;
    unsigned char buf[0x40000];
    int i;
    int npar;
    unsigned int rflag = 0, wflag = 0, padr = 0;
    unsigned int sig1, sig2;

    if (argc != 2) {
        printf("\n No MIBIB file name specified\n");
        return;
    }

    in = fopen(argv[1], "rb");
    if (in == 0) {
        printf("\n Error opening MIBIB file\n");
        return;
    }

    // Search for the start of the partition table block
    while (!feof(in)) {
        fread(&sig1, 1, 4, in);
        fread(&sig2, 1, 4, in);
        if ((sig1 == 0xfe569fac) && (sig2 == 0xcd7f127a)) break; // Found the signature of the partition table block
        fseek(in, 504, SEEK_CUR);  // Skip the rest of the sector
        padr += 512;
    }

    if (feof(in)) {
        printf("\n Partition table block signature not found\n");
        fclose(in);
        return;
    }

    // Now, padr is the address of the partition table block within the file
    // Read the entire MIBIB up to this address - it will be SBL1 with a tail
    fseek(in, 0, SEEK_SET);
    fread(buf, padr, 1, in);

    // Search for the actual tail of the partition
    for (i = padr - 1; i > 0; i--) {
        if (buf[i] != 0xff) break;
    }

    out = fopen("sbl1.bin", "wb");
    fwrite(buf, i + 1, 1, out);
    fclose(out);

    // Find the signatures of the partition tables
    fseek(in, padr, SEEK_SET);

    while (!feof(in)) {
        fread(&sig1, 1, 4, in);
        fread(&sig2, 1, 4, in);

        if ((!rflag) && (sig1 == 0x55ee73aa) && (sig2 == 0xe35ebddb)) {
            // Found the read table
            fseek(in, -8, SEEK_CUR); // Move to the beginning of the signature
            fread(buf, 2048, 1, in);
            npar = *((unsigned int*)&buf[12]); // Number of partitions
            out = fopen("ptable-r.bin", "wb");
            fwrite(buf, 16 + 28 * npar, 1, out);
            fclose(out);
            rflag = 1;  // We have the read table
            continue; // Skip further processing of the sector
        }

        if ((!wflag) && (sig1 == 0xaa7d1b9a) && (sig2 == 0x1f7d48bc)) {
            // Found the write table
            fseek(in, -8, SEEK_CUR); // Move to the beginning of the signature
            fread(buf, 2048, 1, in);
            npar = *((unsigned int*)&buf[12]); // Number of partitions
            out = fopen("ptable-w.bin", "wb");
            fwrite(buf, 16 + 28 * npar, 1, out);
            fclose(out);
            wflag = 1;  // We have the write table
            continue; // Skip further processing of the sector
        }

        if (rflag && wflag) {
            fclose(in);
            return;   // Both tables have been found
        }

        fseek(in, 504, SEEK_CUR);  // Skip the rest of the sector
    }

    if (!rflag) printf("\n Read table not found\n");
    if (!wflag) printf("\n Write table not found\n");
    fclose(in);
}
