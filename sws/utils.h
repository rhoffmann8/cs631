#ifndef _UTILS_H_
#define _UTILS_H_

/* Header file for utility functions used by the server. */

int file_in_dir(DIR*, ino_t, ino_t, char*, int);
int file_in_root(char*);
char* sws_file_path(char*, char*);
int strrchr_pos(char*, char, int);

#endif
