#ifndef __INCLUDE_H__
    #define __INCLUDE_H__

#include <stdint.h>
typedef int32_t int32;
typedef uint32_t uint32;
typedef uint8_t uint8;
typedef int8_t int8;
typedef int16_t int16;
typedef uint16_t uint16;

#ifdef __GNUC__
        #define PACKED_ON(name) struct __attribute__ ((__packed__)) name
        #define PACKED_OFF
#else
        #define PACKED_ON(name) __pragma(pack(push, 1)) struct name
        #define PACKED_OFF __pragma(pack(pop))
#endif

    #include <errno.h>
    #include <fcntl.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <getopt.h>

    #ifdef WIN32
        #ifndef NO_IO
            #include <io.h>
        #endif
        #include <windows.h>
        #define usleep(n) Sleep(n/1000)
    #else
        #include <termios.h>
        #include <unistd.h>
        #include <readline/readline.h>
        #include <readline/history.h>
    #endif

    #ifndef __GNUC__
		#define S_IFMT  170000
		#define S_IFDIR 40000
		#define S_IFBLK 60000
		#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
    #endif
    #include "qcio.h"
    #include "ptable.h"
    #include "utils.h"
#endif
