#include "include.h"

// Boot block size
#define dlblock 1017

//****************************************************
//*  Extract partition tables into separate files
//****************************************************
void extract_ptable() {
  
  FILE* out;

  // Load the system partition table from MIBIB
  load_ptable("@");
  printf("-----------------------------------------------------");
  
  // Check the table
  if (!validpart) {
    printf("\nSystem partition table not found\n");
    return;
  }
  
  out = fopen("ptable/current-r.bin", "wb");
  
  if (out == NULL) {
    printf("\nError opening output file ptable/current-r.bin");
    return;
  }  
  
  fwrite(&fptable, sizeof(fptable), 1, out);
  printf("\n * Found read mode partition table");
  fclose(out);
 /*
// Ищем таблицу записи
for (pg=pg+1;pg<ppb;pg++) {
  flash_read(blk, pg, 0);  // сектор 0 - начало таблицы разделов    
  memread(buf,sector_buf, udsize);
  if (memcmp(buf,"\x9a\x1b\x7d\xaa\xbc\x48\x7d\x1f",8) != 0) continue; // сигнатура не найдена - ищем дальше
  // нашли таблицу записи   
  mempoke(nand_exec,1);     // сектор 1 - продолжение таблицы
  nandwait();
  memread(buf+udsize,sector_buf, udsize);
  npar=*((unsigned int*)&buf[12]); // число разделов в таблице
  out=fopen("ptable/current-w.bin","wb");
  if (out == 0) {
    printf("\n Ошибка открытия выходного файла ptable/current-w.bin");
    return;
  }  
  fwrite(buf,16+28*npar,1,out);
  fclose (out);
  printf("\n * Найдена таблица разделов режима записи");
  return;
}
printf("\n - Таблица разделов режима записи не найдена");
*/
  printf("\n");
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@2
void main(int argc, char* argv[]) {

  int opt;
  unsigned int start = 0;
#ifndef WIN32
  char devname[50] = "/dev/ttyUSB0";
#else
  char devname[50] = "";
#endif
  FILE* in;
  struct stat fstatus;
  unsigned int i, partsize, iolen, adr, helloflag = 0;
  unsigned int sahara_flag = 0;
  unsigned int tflag = 0;
  unsigned int ident_flag = 0, iaddr, ichipset;
  unsigned int filesize;

  unsigned char iobuf[4096];
  unsigned char cmd1[] = {0x06};
  unsigned char cmd2[] = {0x07};
  unsigned char cmddl[2048] = {0xf};
  unsigned char cmdstart[2048] = {0x5, 0, 0, 0, 0};
  unsigned int delay = 2;

  while ((opt = getopt(argc, argv, "p:k:a:histd:q")) != -1) {
    switch (opt) {
      case 'h':
        printf("\n This utility is intended for loading (E)NPRG firmware programs into the modem's memory.\n\n\
The following options are available:\n\n\
-p <tty>  - Specifies the name of the serial port device transferred to download mode.\n\
-i        - Launches the HELLO procedure for bootloader initialization.\n\
-q        - Launches the HELLO procedure in simplified mode without configuring the registers.\n\
-t        - Extracts partition tables from the modem into ptable/current-r(w).bin files.\n\
-s        - Use the SAHARA protocol.\n\
-k #      - Chipset code (-kl - get a list of codes).\n\
-a <adr>  - Load address, default is 0x41700000.\n\
-d <n>    - Delay for bootloader initialization, 0.1s.\n\
\n");
        return;
     
      case 'p':
        strcpy(devname, optarg);
        break;

      case 'k':
        define_chipset(optarg);
        break;

      case 'i':
        helloflag = 1;
        break;
    
      case 'q':
        helloflag = 2;
        break;
    
      case 's':
        sahara_flag = 1;
        break;
    
      case 't':
        tflag = 1;
        break;
    
      case 'a':
        sscanf(optarg, "%x", &start);
        break;

      case 'd':
        sscanf(optarg, "%u", &delay);
        break;

      case '?':
      case ':':  
        return;
    }
  }

  if ((tflag == 1) && (helloflag == 0)) {
    printf("\n The -t option cannot be used without the -i option.\n");
    exit(1);
  }

  delay *= 100000; // Convert to microseconds
  
#ifdef WIN32
  if (*devname == '\0') {
    printf("\n - Serial port is not specified.\n"); 
    return; 
  }
#endif

  if (!open_port(devname))  {
#ifndef WIN32
    printf("\n - Serial port %s cannot be opened.\n", devname); 
#else
    printf("\n - Serial port COM%s cannot be opened.\n", devname); 
#endif
    return; 
  }

  // Remove old partition tables
  unlink("ptable/current-r.bin");
  unlink("ptable/current-w.bin");

  // If the chipset is already defined, determine the Sahara mode
  if (chip_type >= 0)
    sahara_flag = get_sahara();

  if (!sahara_flag) {
    // Open the input file
    in = fopen(argv[optind], "rb");
    if (in == NULL) {
      printf("\nFailed to open the input file.\n");
      return;
    } 

    // Identify the bootloader
    // Look for the identification block
    fseek(in, -12, SEEK_END);
    fread(&i, 4, 1, in);

    if (i == 0xdeadbeef) {
      // Found the block - parse it
      printf("\nFound bootloader identification block");
      fread(&ichipset, 4, 1, in);
      fread(&iaddr, 4, 1, in);
      ident_flag = 1;
      if (start == 0) start = iaddr;
      if (chip_type < 0) set_chipset(ichipset);  // Change the chipset type to the one defined in the identification block
    }
    rewind(in);
  } 

  // Check the chipset type
  if ((chip_type < 0) && (helloflag == 1)) {
    printf("\nChipset type is not specified - full initialization is not possible.\n");
    helloflag = 2;
  }

if ((helloflag == 0)&& (chip_type >= 0))  printf("\n Чипсет: %s",get_chipname());

//printf("\n chip_type = %i   sahara = %i",chip_type,sahara_flag);

if ((start == 0) && !sahara_flag) {
  printf("\nLoad address is not specified.\n");
  fclose(in);
  return;
}  

//----- Loading via Sahara -------

if (sahara_flag) {
  if (dload_sahara() == 0) {
#ifndef WIN32
    usleep(200000);   // Wait for the bootloader initialization
#else
    Sleep(200);   // Wait for the bootloader initialization
#endif

    if (helloflag) {
      hello(helloflag);
      printf("\n");
      if (tflag && (helloflag != 2)) extract_ptable();  // Extract partition tables
    }
  }
  return;
} 

//------- Loading via writing bootloader to memory ----------

printf("\nBootloader file: %s\nLoad address: %08x", argv[optind], start);
iolen = send_cmd_base(cmd1, 1, iobuf, 1);
if (iolen != 5) {
   printf("\nThe modem is not in download mode.\n");
   //dump(iobuf,iolen,0);
   fclose(in);
   return;
}   
iolen = send_cmd_base(cmd2, 1, iobuf, 1);
#ifndef WIN32
fstat(fileno(in), &fstatus);
#else
fstat(_fileno(in), &fstatus);
#endif
filesize = fstatus.st_size;
if (ident_flag) filesize -= 12; // Remove the identification block
printf("\nFile size: %i\n", (unsigned int)filesize);
partsize = dlblock;

// Block-by-block loading loop
for(i = 0; i < filesize; i += dlblock) {  
 if ((filesize - i) < dlblock) partsize = filesize - i;
 fread(cmddl + 7, 1, partsize, in);  // Read the block directly into the command buffer
 adr = start + i;                   // Load address for this block

   // As usual with these cheap Chinese devices, the numbers are stored in Big Endian format
   // Write the load address for this block
   cmddl[1] = (adr >> 24) & 0xff;
   cmddl[2] = (adr >> 16) & 0xff;
   cmddl[3] = (adr >> 8) & 0xff;
   cmddl[4] = (adr) & 0xff;
   // Write the block size
   cmddl[5] = (partsize >> 8) & 0xff;
   cmddl[6] = (partsize) & 0xff;
 iolen = send_cmd_base(cmddl, partsize + 7, iobuf, 1);

 if(iobuf[1] == 0x02)
     printf("\rLoaded: %i", i + partsize);
 else
 {
     printf("\nLoad error\n--\n");
     dump(iobuf, iolen, 0);
     printf("\n--\n");
     return;
 }
}

printf(" OK\n");

// Write the address in the startup command
printf("\nBooting bootloader..."); fflush(stdout);
cmdstart[1] = (start >> 24) & 0xff;
cmdstart[2] = (start >> 16) & 0xff;
cmdstart[3] = (start >> 8) & 0xff;
cmdstart[4] = (start) & 0xff;
iolen = send_cmd_base(cmdstart, 5, iobuf, 1);
close_port();

#ifndef WIN32
usleep(delay);   // Wait for the bootloader initialization
#else
Sleep(delay/1000);   // Wait for the bootloader initialization
#endif

printf("ok\n");

if (helloflag) {
  if (!open_port(devname))  {
#ifndef WIN32
     printf("\n - Serial port %s cannot be opened.\n", devname); 
#else
     printf("\n - Serial port COM%s cannot be opened.\n", devname); 
#endif
     fclose(in);
     return; 
  }

  hello(helloflag);
  if (helloflag != 2)
     if (!bad_loader && tflag) extract_ptable();  // Extract partition tables
}  

printf("\n");
fclose(in);
}
