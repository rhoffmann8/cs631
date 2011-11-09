/*
 * print.h - Header file containing print_info and max_widths structure
 * definitions and printing function prototypes.
 */

#ifndef _PRINT_H_
#define _PRINT_H_

struct print_info {
	char *name;
	char *accpath;
	char *path;
	char *parent_path;
	struct stat *statp;
	struct print_info *next;
};

struct max_widths {
	int blksize;
	int gid;
	int inode;
	int major;
	int minor;
	int name;
	int size;
	int uid;
};

char get_F_symbol(struct stat*);
void print(struct print_info*);
void print_long(struct print_info*);
char* print_the_unprintable(char*);

#endif
