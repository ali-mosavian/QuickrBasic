#ifndef DIRENT_H
#define DIRENT_H
struct dirent {
    char d_name[256];
};
typedef struct DIR DIR;
DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);
#endif
