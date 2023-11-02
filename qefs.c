#ifndef WIN64
#define _USE_32BIT_TIME_T
#endif
#include "include.h"
#include <time.h>
#include "efsio.h"

//%%%%%%%%% Common Variables %%%%%%%%%%%%%%%%

unsigned int fixname = 0; // Indicator for explicitly specifying a file name
char filename[50];        // Output file name

char* fbuf;  // Buffer for file operations

int recurseflag = 0;
int fullpathflag = 0;

char iobuf[44096];
int iolen;

#ifdef WIN32
struct tm* localtime_r(const time_t* clock, struct tm* result) {
    if (!clock || !result) return NULL;
    memcpy(result, localtime(clock), sizeof(*result));
    return result;
}
#endif

// File listing modes:

enum {
  fl_tree,    // Tree
  fl_ftree,   // Tree with files
  fl_list,    // File listing
  fl_full,    // Full file listing
  fl_mid      // MC extfs format file listing
};

int tspace; // Indentation for generating a file tree

//****************************************************
//* Read EFS dump (efs.mbn)
//****************************************************

void back_efs() {
  FILE* out;
  struct efs_factimage_rsp rsp;
  rsp.stream_state = 0;
  rsp.info_cluster_sent = 0;
  rsp.cluster_map_seqno = 0;
  rsp.cluster_data_seqno = 0;

  strcpy(filename, "efs.mbn");
  out = fopen(filename, "w");
  if (out == 0) {
    printf("\nError opening the output file %s\n", filename);
    return;
  }

  if (efs_prep_factimage() != 0) {
    printf("\n Error entering Factory Image mode, code %d\n", efs_get_errno());
    fclose(out);
    return;
  }

  if (efs_factimage_start() != 0) {
    printf("\n Error starting EFS reading, code %d\n", efs_get_errno());
    fclose(out);
    return;
  }

  printf("\n");

  // Main loop for getting efs.mbn
  while (1) {
    printf("\r Reading: sent=%i map=%i data=%i", rsp.info_cluster_sent, rsp.cluster_map_seqno, rsp.cluster_data_seqno);
    fflush(stdout);
    if (efs_factimage_read(rsp.stream_state, rsp.info_cluster_sent, rsp.cluster_map_seqno,
                          rsp.cluster_data_seqno, &rsp) != 0) {
      printf("\n Error reading, code=%d\n", efs_get_errno());
      return;
    }
    if (rsp.stream_state == 0) break; // End of data stream
    fwrite(rsp.page, 512, 1, out);
  }
  // Close EFS
  efs_factimage_end();
  fclose(out);
}

//***************************************************
//* Output file name with tree indentation
//***************************************************
void printspace(char* name) {
  int i;
  printf("\n");
  if (tspace != 0)
    for (i = 0; i < tspace * 3; i++) printf(" ");
  printf("%s", name);
}

//*********************************************
//* Output file access attribute
//*********************************************
static char atrstr[15];

void fattr(int mode, char* str) {
  memset(str, '-', 3);
  str[3] = 0;
  if ((mode & 4) != 0) str[0] = 'r';
  if ((mode & 2) != 0) str[1] = 'w';
  if ((mode & 1) != 0) str[2] = 'x';
}

//*********************************************
char* cfattr(int mode) {
  char str[5];
  atrstr[0] = 0;
  fattr((mode >> 6) & 7, str);
  strcat(atrstr, str);
  fattr((mode >> 3) & 7, str);
  strcat(atrstr, str);
  fattr(mode & 7, str);
  strcat(atrstr, str);
  return atrstr;
}

//****************************************************
//* Get the symbolic description of a file attribute
//****************************************************
char* str_filetype(int attr, char* buf) {
  strcpy(buf, "Unknown");
  if ((attr & S_IFMT) == S_IFIFO) strcpy(buf, "fifo");
  else if ((attr & S_IFMT) == S_IFCHR) strcpy(buf, "Character device");
  else if ((attr & S_IFMT) == S_IFDIR) strcpy(buf, "Directory");
  else if ((attr & S_IFMT) == S_IFBLK) strcpy(buf, "Block device");
  else if ((attr & S_IFMT) == S_IFREG) strcpy(buf, "Regular file");
  else if ((attr & S_IFMT) == S_IFLNK) strcpy(buf, "Symlink");
  else if ((attr & S_IFMT) == S_IFSOCK) strcpy(buf, "Socket");
  else if ((attr & S_IFMT) == S_IFITM) strcpy(buf, "Item File");
  return buf;
}

//****************************************************
//* Get the one-character description of a file attribute
//****************************************************
char chr_filetype(int attr) {
  if ((attr & S_IFMT) == S_IFIFO) return 'p';
  else if ((attr & S_IFMT) == S_IFCHR) return 'c';
  else if ((attr & S_IFMT) == S_IFDIR) return 'd';
  else if ((attr & S_IFMT) == S_IFBLK) return 'b';
  else if ((attr & S_IFMT) == S_IFREG) return '-';
  else if ((attr & S_IFMT) == S_IFLNK) return 'l';
  else if ((attr & S_IFMT) == S_IFSOCK) return 's';
  else if ((attr & S_IFMT) == S_IFITM) return 'i';
  return '-';
}

//****************************************************
//* Convert date to ASCII string
//* Format:
//* 0 - Normal
//* 1 - For Midnight Commander
//****************************************************
char* time_to_ascii(int32 time, int format) {
  time_t xtime;      // Same time but with time_t type
  struct tm lt;      // Structure for storing the converted date
  static char timestr[100];

  xtime = time;
  if (localtime_r(&xtime, &lt) != 0) {
    if (format == 0) strftime(timestr, 100, "%d-%b-%y %H:%M", localtime(&xtime));
    else strftime(timestr, 100, "%m-%d-%y %H:%M", localtime(&xtime));
  } else strcpy(timestr, "---------------");
  return timestr;
}

//****************************************************
//* Display detailed information about a regular file
//****************************************************
void show_efs_filestat(char* filename, struct efs_filestat* fi) {
  
  char sfbuf[50]; // Buffer to save the file type description

  printf("\n File Name: %s", filename);
  printf("\n Size: %i bytes", fi->size);
  printf("\n File Type: %s", str_filetype(fi->mode, sfbuf));
  printf("\n Link Count: %d", fi->nlink);
  printf("\n Access Attributes: %s", cfattr(fi->mode));
  printf("\n Creation Date: %s", time_to_ascii(fi->ctime, 0));
  printf("\n Modification Date: %s", time_to_ascii(fi->mtime, 0));
  printf("\n Last Access Date: %s\n", time_to_ascii(fi->atime, 0));
}

//****************************************************
//* Display the file tree of the specified directory
//* lmode - output mode fl_*:
//*   fl_tree   - tree
//*   fl_ftree, - tree with files
//* fname - starting path, default is /
//****************************************************
void show_tree(int lmode, char* fname) {
  struct efs_dirent dentry; // Directory item descriptor
  unsigned char dirname[100];	
  int dirp = 0;  // Pointer to the open directory

  int i, nfile;
  char targetname[200];

  if (strlen(fname) == 0) strcpy(dirname, "/"); // Open the root directory by default
  else strcpy(dirname, fname);

  // chdir
  dirp = efs_opendir(dirname);
  if (dirp == 0) {
    printf("\n ! Access to directory %s is denied, errno=%i\n", dirname, efs_get_errno());
    return;
  }

  // Loop for selecting directory entries
  for (nfile = 1;; nfile++) {
    // Select the next entry
    if (efs_readdir(dirp, nfile, &dentry) == -1) continue; // When there's an error reading the next structure
    if (dentry.name[0] == 0) break;   // End of the list

    // Determine the full file name
    strcpy(targetname, dirname);
    //strcat(targetname,"/");
    strcat(targetname, dentry.name); // Skip the initial "/"
    if (dentry.entry_type == 1) strcat(targetname, "/"); // This is a directory

    if ((lmode == fl_tree) && (dentry.entry_type != 1)) continue; // Skip regular files in directory tree mode

    if (fullpathflag) printspace(targetname);
    else {
      for (i = strlen(targetname) - 2; i >= 0; i--) {
        if (targetname[i] == '/') break;
      } 
      i++;
      printspace(targetname + i);
    }  
    if (dentry.entry_type == 1) {
      // This entry is a directory - process the nested subdirectory
      tspace++;
      efs_closedir(dirp);
      show_tree(lmode, targetname); 
      dirp = efs_opendir(dirname);
      tspace--;
    }  
  }
  efs_closedir(dirp); 
}

//****************************************************
//* Display the file list of the specified directory
//* lmode - output mode fl_*:
//*   fl_list - concise file listing
//*   fl_full - full file listing
//*   fl_mid  - full file listing in midnight commander format
//* fname - starting path, default is /
//****************************************************
void show_files(int lmode, char* fname) {
  struct efs_dirent dentry; // Directory item descriptor
  char dnlist[200][100]; // List of directories
  unsigned short ndir = 0;
  unsigned char dirname[100];	
  int dirp = 0;  // Pointer to the open directory

  int i, nfile;
  char ftype;
  char targetname[200];

  if (strlen(fname) == 0) strcpy(dirname, "/"); // Open the root directory by default
  else strcpy(dirname, fname);

  // opendir
  dirp = efs_opendir(dirname);
  if (dirp == 0) {
    if (lmode != fl_mid) printf("\n ! Access to directory %s is denied, errno=%i\n", dirname, efs_get_errno());
    //printf("\n ! Доступ в каталог %s запрещен\n",dirname);
    return;
  }
  if (lmode == fl_full) printf("\n *** Directory %s ***\n", dirname);

  // Search for files
  for (nfile = 1;; nfile++) {
    // Select the next entry
    if (efs_readdir(dirp, nfile, &dentry) == -1) {
      continue; // When there's an error reading the next structure
    }  
    if (dentry.name[0] == 0) {
      break;   // End of the list
    }  
    ftype = chr_filetype(dentry.mode);
    if ((dentry.entry_type) == 1) { 
      // Form a list of subdirectories
      strcpy(dnlist[ndir++], dentry.name);
    }  

    // Determine the full file name
    strcpy(targetname, dirname);
    //strcat(targetname,"/");
    strcat(targetname, dentry.name); // Skip the initial "/"
    if (ftype == 'd') strcat(targetname, "/");

    // Simple file list mode
    if (lmode == fl_list) {
      printf("\n%s", targetname);
      if ((ftype == 'd') && (recurseflag == 1)) { 
        show_files(lmode, targetname);
      } 
      continue;
    }

    // Full file list mode
    if (lmode == fl_full) 
      printf("%c%s %9i %s %s\n",
          ftype,
          cfattr(dentry.mode),
          dentry.size,
          time_to_ascii(dentry.mtime, 0),
          dentry.name);

    // Midnight Commander mode
    if (lmode == fl_mid) {
      if (ftype == 'i') ftype = '-';
      printf("%c%s",
          ftype,                          // attr
          cfattr(dentry.mode));           // mode

      printf(" %d root root", ftype == 'd' ? 2 : 1);     // nlink, owner
      printf(" %9d %s %s/%s\n", 
        dentry.size,                   // size
          time_to_ascii(dentry.mtime, 1),      // date
          dirname,	 
          dentry.name);                     // name
    }
  }

  // This directory has been processed - process nested subdirectories in full view mode
  efs_closedir(dirp);  
  if (lmode == fl_full) printf("\n  * Files: %i\n", nfile);
  if (((lmode == fl_full) && recurseflag) || (lmode == fl_mid)) {
    for (i = 0; i < ndir; i++) {
      strcpy(targetname, dirname);
      strcat(targetname, "/");
      strcat(targetname, dnlist[i]);
      show_files(lmode, targetname);
    }
  }  
}
  
//**************************************************
//* Read a file into a buffer
//**************************************************
unsigned int readfile(char* filename) {	

  struct efs_filestat fi;
  int i, blk;
  int fd;

  efs_close(1);
  switch (efs_stat(filename, &fi)) {
    case 0:
      printf("\nObject %s not found\n", filename);
      return 0;

    case 2: // Directory
      printf("\nObject %s is a directory\n", filename);
      return 0;
  }

  if (fi.size == 0) {
    printf("\nFile %s contains no data\n", filename);
    return 0;
  }

  fbuf = malloc(fi.size);
  fd = efs_open(filename, O_RDONLY);
  if (fd == -1) {
    printf("\nError opening the file %s", filename);
    return 0;
  }

  blk = 512;
  for (i = 0; i < (fi.size); i += 512) {
    if ((i + 512) > fi.size) blk = fi.size - i;
    if (efs_read(fd, fbuf + i, blk, i) <= 0) 
      return 0; // Read error
  }

  efs_close(fd);
  return fi.size;
}

//**************************************************
//* Write a file
//**************************************************
unsigned int write_file(char* file, char* path) {	

  struct efs_filestat fi;
  int i, blk;
  FILE* in;
  long filesize;
  int fd;

  efs_close(1);

  // Prepare the output file name
  strcpy(filename, path);

  // Check for the existence of the file
  switch (efs_stat(filename, &fi)) {
    case 1:
      printf("\nObject %s already exists\n", filename);
      return 0;

    case 2: // Directory
      strcat(filename, "/");
      strcat(filename, file);
  }    

  // Read the file into a buffer
  in = fopen(file, "r");
  if (in == 0) {
    printf("\nError opening the file %s", file);
    return 0;
  }

  fseek(in, 0, SEEK_END);
  filesize = ftell(in);
  fbuf = malloc(filesize);
  fseek(in, 0, SEEK_SET);
  fread(fbuf, 1, filesize, in);
  fclose(in);

  fd = efs_open(filename, O_CREAT);
  if (fd == -1) {
    printf("\nError opening the file %s for writing", filename);
    return 0;
  }

  blk = 512;
  for (i = 0; i < (filesize); i += 512) {
    if ((i + 512) > filesize) {
      blk = filesize - i;
    }
    efs_write(fd, fbuf + i, blk, i);
    usleep(3000);
  }
  free(fbuf);
  efs_close(fd);
  return 1;
}

//***************************************
//* View a file on the screen
//*
//* mode=0 - view as text
//*      1 - view as a dump
//***************************************
void list_file(char* filename, int mode) {
  
  unsigned int flen;

  flen = readfile(filename);
  if (flen == 0) {
    printf("Error reading the file %s", filename);
    return;
  }  

  if (!mode) fwrite(fbuf, flen, 1, stdout);
  else dump(fbuf, flen, 0);

  free(fbuf);
}

//******************************************************
//* Copy a single file from EFS to the current directory
//******************************************************
void get_file(char* name, char* dst) {
  
  unsigned int flen;
  char* fnpos;
  FILE* out;
  struct stat fs;
  char filename[200];

  flen = readfile(name);
  if (flen == 0) {
    printf("Error reading the file %s", filename);
    return;
  }

  // Extract the file name from the full path
  fnpos = strrchr(name, '/');
  if (fnpos == 0) fnpos = name;
  else fnpos++;

  if (dst == 0) {
    // No output directory or file is specified
    strcpy(filename, fnpos);
  }
  else {
    // Output directory or file is specified
    if ((stat(dst, &fs) == 0) && S_ISDIR(fs.st_mode)) {
      // The output file is a directory
      strcpy(filename, dst);
      strcat(filename, "/");
      strcat(filename, fnpos);
    }
    // The output file doesn't exist or is a regular file
    else strcpy(filename, dst);
  }      

  out = fopen(filename, "w");
  if (out == 0) {
    printf("Error opening the output file\n");
    exit(1);
  }  
  fwrite(fbuf, 1, flen, out);
  fclose(out);
}

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@ Main Program
void main(int argc, char* argv[]) {

  unsigned int opt;
  int i;
  struct efs_filestat fi;
  char filename[100];

  enum {
    MODE_BACK_EFS,
    MODE_FILELIST,
    MODE_TYPE,
    MODE_GETFILE,
    MODE_WRITEFILE,
    MODE_DELFILE,
    MODE_MKDIR
  };

  enum {
    T_TEXT,
    T_DUMP
  };

  enum {
    G_FILE,
    G_ALL,
    G_DIR
  };

  int mode = -1;
  int lmode = -1;
  int tmode = -1;
  int gmode = -1;

#ifndef WIN32
  char devname[50] = "/dev/ttyUSB0";
#else
  char devname[50] = "";
#endif

  while ((opt = getopt(argc, argv, "hp:o:ab:g:l:rt:w:e:fm:")) != -1) {
    switch (opt) {
      case 'h':
        printf("\n  This utility is designed to work with an EFS partition\n\
%s [options] [path or filename] [output filename]\n\
Supported options:\n\n\
* Options specifying the operation to perform:\n\
-be       - EFS dump\n\n\
-ld       - Show the EFS directory tree (excluding regular files)\n\
-lt       - Show the EFS directory and file tree\n\
-ll       - Show a short list of files in the specified directory\n\
-lf       - Show a full list of files in the specified directory\n  (For all -l options, you can specify the initial path to the directory)\n\n\
-tt       - View a file as text\n\
-td       - View a file as a dump\n\n\
-gf file [dst] - Read the specified file from EFS into the file dst or the current directory\n\
-wf file path - Write the specified file to the specified path\n\
-ef file  - Delete the specified file\n\
-ed dir   - Delete the specified directory\n\
-md dir   - Create a directory with the specified access permissions\n\n\
* Modifier options:\n\
-r        - Process all subdirectories when listing\n\
-f        - Show the full path for each directory when viewing the tree\n\
-p <tty>  - Specify the name of the diagnostic port device for the modem\n\
-a        - Use the alternative EFS\n\
-o <file> - Specify the filename for saving EFS\n\
\n", argv[0]);
        return;

      case 'o':
        strcpy(filename, optarg);
        fixname = 1;
        break;

      //  === Backup group keys ==
      case 'b':
        if (mode != -1) {
          printf("\n More than 1 operating mode key is specified in the command line\n");
          return;
        }
        switch (*optarg) {
          case 'e':
            mode = MODE_BACK_EFS;
            break;

          default:
            printf("\n Invalid value specified for key -b\n");
            return;
        }
        break;

      // File list
      case 'l':
        if (mode != -1) {
          printf("\n More than 1 operating mode key is specified in the command line\n");
          return;
        }
        mode = MODE_FILELIST;
        switch (*optarg) {
          case 'd':
            lmode = fl_tree;
            break;
          case 't':
            lmode = fl_ftree;
            break;
          case 'l':
            lmode = fl_list;
            break;
          case 'f':
            lmode = fl_full;
            break;
          case 'm':
            lmode = fl_mid;
            break;
          default:
            printf("\n Invalid value specified for key -l\n");
            return;
        }
        break;

      //  === File viewing group keys
      case 't':
        if (mode != -1) {
          printf("\n More than 1 operating mode key is specified in the command line\n");
          return;
        }
        mode = MODE_TYPE;
        switch (*optarg) {
          case 't':
            tmode = T_TEXT;
            break;

          case 'd':
            tmode = T_DUMP;
            break;

          default:
            printf("\n Invalid value specified for key -t\n");
            return;
        }
        break;

      //  === File extraction (get) group keys ==
      case 'g':
        if (mode != -1) {
          printf("\n More than 1 operating mode key is specified in the command line\n");
          return;
        }
        mode = MODE_GETFILE;
        switch (*optarg) {
          case 'f':
            gmode = G_FILE;
            break;

          default:
            printf("\n Invalid value specified for key -g\n");
            return;
        }
        break;

      //  === File writing (write) group keys ==
      case 'w':
        if (mode != -1) {
          printf("\n More than 1 operating mode key is specified in the command line\n");
          return;
        }
        mode = MODE_WRITEFILE;
        switch (*optarg) {
          case 'f':
            gmode = G_FILE;
            break;

          default:
            printf("\n Invalid value specified for key -g\n");
            return;
        }
        break;

      //  === File deletion (erase) group keys ==
      case 'e':
        if (mode != -1) {
          printf("\n More than 1 operating mode key is specified in the command line\n");
          return;
        }
        mode = MODE_DELFILE;
        switch (*optarg) {
          case 'f':
            gmode = G_FILE;
            break;

          case 'd':
            gmode = G_DIR;
            break;

          default:
            printf("\n Invalid value specified for key -g\n");
            return;
        }
        break;

      // ==== Key for creating a directory ====
      case 'm':
        if (mode != -1) {
          printf("\n More than 1 operating mode key is specified in the command line\n");
          return;
        }
        mode = MODE_MKDIR;
        if (*optarg != 'd') {
          printf("\n Invalid key m%c", *optarg);
          return;
        }
        break;

      // ===================== Auxiliary keys ====================
      case 'p':
        strcpy(devname, optarg);
        break;

      case 'a':
        set_altflag(1);
        break;

      case 'r':
        recurseflag = 1;
        break;

      case 'f':
        fullpathflag = 1;
        break;

      case '?':
      case ':':
        return;
    }
  }

  if (mode == -1) {
    printf("\n No operation key is specified\n");
    return;
  }

#ifdef WIN32
  if (*devname == '\0') {
    printf("\n - Serial port is not specified\n");
    return;
  }
#endif

  if (!open_port(devname)) {
#ifndef WIN32
    printf("\n - Serial port %s cannot be opened\n", devname);
#else
    printf("\n - Serial port COM%s cannot be opened\n", devname);
#endif
    return;
  }

  // Close all open directory handles
  for (i = 1; i < 10; i++) efs_closedir(i);

  // Run the necessary procedures depending on the operating mode
  switch (mode) {

    //============================================================================
    // EFS Dump
    case MODE_BACK_EFS:
      back_efs();
      break;

    //============================================================================
    // View directory
    case MODE_FILELIST:
      tspace = 0;
      // Path is not specified - work with the root directory
      if (optind == argc) strcpy(filename, "/");
      // Path is specified
      else strcpy(filename, argv[optind]);
      // Check for the existence of the file and whether it is a directory
      i = efs_stat(filename, &fi);
      switch (i) {
        case 0:
          printf("\nObject %s not found\n", filename);
          break;

        case 1: // Regular file
          show_efs_filestat(filename, &fi);
          break;

        case 2: // Directory
          if ((lmode == fl_tree) || (lmode == fl_ftree)) show_tree(lmode, filename);
          else show_files(lmode, filename);
          break;

        case -1: // Error
          printf("\nObject %s is not accessible, code %d", filename, efs_get_errno());
          break;
      }
      break;

    //============================================================================
    // View files
    case MODE_TYPE:
      if (optind == argc) {
        printf("\n No filename specified");
        break;
      }
      list_file(argv[optind], tmode);
      break;

    //============================================================================
    // Get file
    case MODE_GETFILE:
      if (optind < (argc - 2)) {
        printf("\n Insufficient parameters in the command line");
        break;
      }
      if (optind == (argc - 1)) get_file(argv[optind], 0);
      else get_file(argv[optind], argv[optind + 1]);
      break;

    //============================================================================
    // Write file
    case MODE_WRITEFILE:
      if (optind != (argc - 2)) {
        printf("\n Insufficient parameters in the command line");
        break;
      }
      write_file(argv[optind], argv[optind + 1]);
      break;

    //============================================================================
    // Delete file
    case MODE_DELFILE:
      if (optind == argc) {
        printf("\n No filename specified");
        break;
      }
      switch (gmode) {
        case G_FILE:
          efs_unlink(argv[optind]);
          break;

        case G_DIR:
          efs_rmdir(argv[optind]);
          break;
      }
      break;

    //============================================================================
    // Create directory
    case MODE_MKDIR:
      if (optind == argc) {
        printf("\n Insufficient parameters in the command line");
        break;
      }
      if (efs_mkdir(argv[optind], 7) != 0) {
        printf("\nError creating directory %s, code %d", argv[optind], efs_get_errno());
      }
      break;
      //============================================================================
      default:
        printf("\n Не указан ключ выполняемой операции\n");
        return;
  }

  if (lmode != fl_mid) printf("\n");
}

