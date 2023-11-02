#include "include.h"

//****************************************
//* Download via Sahara
//****************************************
int dload_sahara() {

    FILE* in;
    char infilename[200] = "loaders/";
    unsigned char sendbuf[131072];
    unsigned char replybuf[128];
    unsigned int iolen, offset, len, done_stat, img_id;
    unsigned char hello_reply[60] = {
        02, 00, 00, 00, 48, 00, 00, 00, 02, 00, 00, 00, 01, 00, 00, 00,
        00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00,
        00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00, 00
    };
    unsigned char done_message[8] = {5, 0, 0, 0, 8, 0, 0, 0};

    printf("\nWaiting for the Hello packet from the device...\n");
    port_timeout(100); // We will wait for the Hello packet for 10 seconds
    iolen = qread(siofd, replybuf, 48); // Read Hello
    if ((iolen != 48) || (replybuf[0] != 1)) {
        sendbuf[0] = 0x3a; // Could be any number
        qwrite(siofd, sendbuf, 1); // Initiate sending the Hello packet
        iolen = qread(siofd, replybuf, 48); // Try to read Hello again
        if ((iolen != 48) || (replybuf[0] != 1)) { // Now we have waited long enough
            printf("\nHello packet from the device not received\n");
            dump(replybuf, iolen, 0);
            return 1;
        }
    }

    // Received Hello
    ttyflush(); // Clear the receive buffer
    port_timeout(10); // Now the packet exchange will be faster - 1s timeout
    qwrite(siofd, hello_reply, 48); // Send Hello Response with mode switch
    iolen = qread(siofd, replybuf, 20); // Response packet
    if (iolen == 0) {
        printf("\nNo response from the device\n");
        return 1;
    }
    // replybuf should contain the request for the first loader block
    img_id = *((unsigned int*)&replybuf[8]); // Image identifier
    printf("\nImage identifier for download: %08x\n", img_id);

    switch (img_id) {

    case 0x07:
        strcat(infilename, get_nprg());
        break;

    case 0x0d:
        strcat(infilename, get_enprg());
        break;

    default:
        printf("\nUnknown identifier - no such image!\n");
        return 1;
    }
    printf("\nDownloading %s...\n", infilename);
    fflush(stdout);
    in = qopenfile(infilename, "rb");
    if (in == 0) {
        printf("\nError opening input file %s\n", infilename);
        return 1;
    }

    // Main loop for sending the loader code
    printf("\nTransmitting loader to the device...\n");
    while (replybuf[0] != 4) { // EOIT message
        if (replybuf[0] != 3) { // Read Data message
            printf("\nPacket with an invalid code - aborting the download!");
            dump(replybuf, iolen, 0);
            fclose(in);
            return 1;
        }
        // Extract fragment file parameters
        offset = *((unsigned int*)&replybuf[12]);
        len = *((unsigned int*)&replybuf[16]);
        //printf("\n адрес=%08x длина=%08x",offset,len);
        fseek(in, offset, SEEK_SET);
        fread(sendbuf, 1, len, in);
        // Send the data block to Sahara
        qwrite(siofd, sendbuf, len);
        // Receive the response
        iolen = qread(siofd, replybuf, 20); // Response packet
        if (iolen == 0) {
            printf("\nNo response from the device\n");
            fclose(in);
            return 1;
        }
    }
    // Received EOIT, end of download
    qwrite(siofd, done_message, 8); // Send the Done packet
    iolen = qread(siofd, replybuf, 12); // Wait for Done Response
    if (iolen == 0) {
        printf("\nNo response from the device\n");
        fclose(in);
        return 1;
    }
    // Get the status
    done_stat = *((unsigned int*)&replybuf[12]);
    if (done_stat == 0) {
        printf("\nLoader transferred successfully\n");
    } else {
        printf("\nLoader transmission error\n");
    }
    fclose(in);

    return done_stat;
}
