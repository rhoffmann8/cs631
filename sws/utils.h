#ifndef _UTILS_H_
#define _UTILS_H_

int file_in_dir(DIR*, ino_t, ino_t, char*, int);
int file_in_root(char*);
int sws_file_path(char*, char*, char**);
int strrchr_pos(char*, char, int);

#endif
