#include "include.h"

//--------------------------------------------------------------------------------
//* Terminal program for working with modem AT commands
//--------------------------------------------------------------------------------

unsigned int hexflag = 0;         // Hex mode flag
unsigned int wrapperlen = 0;      // Line length (0 - no line wrap)
unsigned int waittime = 1;        // Response waiting time
unsigned int monitorflag = 0;     // Monitor mode
unsigned int autoflag = 1;        // Automatic addition of 'AT' prefix
char outcmd[500];
char ibuf[6000];

//*****************************************************
//*   Receive and print modem response
//*****************************************************
void read_response() {

    int i;
    int dlen = 0;  // Total response length in the buffer
    int rlen;       // Length of the received part of the response
    int clen;       // Length of the response fragment with the width of the terminal line

    // Loop to receive response parts and assemble the response into a single buffer
    do {
        // usleep(waittime * 100); // Delay for response waiting - not needed, termios handles it automatically
        rlen = qread(siofd, ibuf + dlen, 5000);   // Response of the command
        if ((dlen + rlen) >= 5000) break; // Buffer overflow
        dlen += rlen;
    } while (rlen != 0);

    if (dlen == 0) return; // No response received

    // Response received - display it on the screen
    if (hexflag) {
        printf("\n");
        rlen = -1;
        dump(ibuf, dlen, 0);
        printf("\n");
    }
    else {
        ibuf[dlen] = 0; // End of the string
        printf("\n");
        if (wrapperlen == 0) puts(ibuf);
        else {
            clen = wrapperlen;
            for (i = 0; i < dlen; i += wrapperlen) {
                if ((dlen - i) < wrapperlen) clen = dlen - i; // Length of the last line
                fwrite(ibuf + i, 1, clen, stdout);
                printf("\n");
                fflush(stdout);
            }
        }
    }
    fflush(stdout);
}

//*****************************************************
//*  Send a command to the modem and get the result   *
//*****************************************************
void process_command(char* cmdline) {

    outcmd[0] = 0;

    // Automatic addition of the AT prefix
    if (autoflag &&
        (((cmdline[0] != 'a') && (cmdline[0] != 'A')) ||
        ((cmdline[1] != 't') && (cmdline[1] != 'T') && (cmdline[1] != '/')))
        )  strcpy(outcmd, "AT");
    strcat(outcmd, cmdline);
    strcat(outcmd, "\r");   // Add CR at the end of the string

    // Send the command
    ttyflush();  // Clear the output buffer
    qwrite(siofd, outcmd, strlen(outcmd));  // Send the command

    read_response();
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void main(int argc, char* argv[]) {

#ifndef WIN32
    char* line;
    char oldcmdline[200] = "";
#else
    char line[200];
#endif
    char scmdline[200] = {0};
#ifndef WIN32
    char devname[50] = "/dev/ttyUSB0";
#else
    char devname[50] = "";
#endif
    int opt;

    while ((opt = getopt(argc, argv, "p:xw:c:hd:ma")) != -1) {
        switch (opt) {
        case 'h':
            printf("\nTerminal program for entering AT commands into a modem\n\n\
The following options are available:\n\n\
-p <tty>       - Specifies the name of the serial port device\n\
-d <time>      - Sets the modem response waiting time in ms\n\
-x            - Displays the modem response in HEX dump format\n\
-w <len>      - Line length for wrapping long lines (0 - no wrap)\n\
-m            - Port monitor mode\n\
-a            - Disables automatic addition of 'AT' prefix to the command\n\
-c \"<command>\" - Runs the specified command and exits\n");
            return;

        case 'p':
            strcpy(devname, optarg);
            break;

        case 'c':
            strcpy(scmdline, optarg);
            break;

        case 'd':
            sscanf(optarg, "%i", &waittime);
            break;

        case 'w':
            sscanf(optarg, "%i", &wrapperlen);
            break;

        case 'x':
            hexflag = 1;
            break;

        case 'a':
            autoflag = 0;
            break;

        case 'm':
            monitorflag = 1;
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

    if (!open_port(devname)) {
#ifndef WIN32
        printf("\n - Serial port %s does not open\n", devname);
#else
        printf("\n - Serial port COM%s does not open\n", devname);
#endif
        return;
    }

    // Set port timeout
    port_timeout(waittime);

    // Monitor mode
    if (monitorflag)
        for (;;)
            read_response();

    // Run a command from the -c option, if provided
    if (strlen(scmdline) != 0) {
        process_command(scmdline);
        return;
    }

    // Main command processing loop
#ifndef WIN32
    // Load command history
    read_history("qcommand.history");
    write_history("qcommand.history");
#endif

    for (;;) {
#ifndef WIN32
        line = readline(">");
#else
        printf(">");
        fgets(line, sizeof(line), stdin);
#endif
        if (line == 0) {
            printf("\n");
            return;
        }
        if (strlen(line) == 0) continue; // Empty command
#ifndef WIN32
        if (strcmp(line, oldcmdline) != 0) {
            add_history(line); // Add to the command history buffer
            append_history(1, "qcommand.history");
            strcpy(oldcmdline, line);
        }
#endif
        process_command(line);
#ifndef WIN32
        free(line);
#endif
    }
}
