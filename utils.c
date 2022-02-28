#include "include.h"

FILE * qopenfile(char *fname, char *mode)
{
    FILE * file = NULL;
    const int bufsz = 4096;
#ifdef WIN32
    LPWSTR path = malloc(bufsz*sizeof(WCHAR));
    int len = GetModuleFileNameW(NULL, path, bufsz);
    if(len > 0 && len < bufsz)
    {
        size_t fname_sz = strlen(fname)+1;
        wchar_t wfname[fname_sz];
        mbstowcs(wfname, fname, fname_sz);
        for(; len > 0; len--)
            if(path[len-1] == L'\\')
            {
                path[len] = L'\0';
                break;
            }
        if(bufsz >= len+sizeof(wfname))
        {
            size_t mode_sz = strlen(mode)+1;
            wchar_t wmode[mode_sz];
            mbstowcs(wmode, mode, mode_sz);
            wcscat(path, wfname);
            file = _wfopen(path, wmode);
        }
    }
#else
    char *path = malloc(bufsz);
    ssize_t len = readlink("/proc/self/exe", path, bufsz);
    if(len > 0)
    {
       for(; len > 0; len--)
            if(path[len-1] == '/')
            {
                path[len] = '\0';
                break;
            }
        if(len == 0)
            path[0] = '\0';
        if(bufsz >= len+strlen(fname)+1)
        {
            strcat(path, fname);
            file = fopen(path, mode);
        }
    }
#endif
    free(path);
    if(!file)
        file = fopen(fname, mode);
    return file;
}
