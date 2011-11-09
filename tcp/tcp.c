/*
 * Rob Hoffmann, CS631
 * Fall 2011
 * Assignment #2 - tcp (trivially copy a file)
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef BUFF_SIZE
#define BUFF_SIZE 8192
#endif

int copy(char*, const char*);
static void usage(void);
int main(int, char *[]);

/*
 * Program to trivially copy a file. Main routine invokes copy function if
 * passed command line arguments are correct. Copy function then attempts to
 * open both the source and destination files (creating the destination file
 * if necessary), and then attempts to read contents of source file and write
 * into destination file.
 *
 * Program returns EXIT_SUCCESS upon success, otherwise an error is displayed
 * and program returns EXIT_FAILURE. If usage() is called, it calls
 * exit(EXIT_FAILURE) and the program terminates there.
 */
int
main(int argc, char *argv[])
{
	int i;
	int err;

	if (argc != 3) {
    		usage();
		/* NOTREACHED */
  	}

	i = optind;
	if ((err = copy(argv[i], argv[i+1])) > 0) {
		switch(err) {
			case 1:
				perror(argv[i]);
				break;
			case 2:
				perror(argv[i+1]);
				break;
			case 3:
				fprintf(stderr,
					"%s: Source cannot be a directory\n", 
					argv[i]);
				break;
			case 4:
				fprintf(stderr, 
					"Cannot copy file to itself!\n");
				break;
			default:
				perror("error");
				break;
		}
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/*
 * Copy function. Takes source path and destination path strings and attempts
 * to open file handlers on the supplied paths. If successful, attempts to read
 * characters from source file and write them to destination file. Returns 0 on
 * success, and a specific code on error.
 * Returns the following error codes if an error occurs:
 * 	1 if an error with source
 * 	2 if an error with destination
 * 	3 in order to use a cusotm message with source
 * 	4 in order to use a custom message for copying file to itself
 * Error codes are handled in the main function with perror.
 *
 * Source path cannot be a directory, but destination path can. A destination
 * file that exists is overwritten, similar to the behavior of cp.
 */
int
copy(char* src, const char* dst)
{
	struct stat stat_buff;
	ino_t inode;
	mode_t perms;
	int dstfd, srcfd, n;
	void *buff;
	char *newpath, *tmp;

	/* Allocate memory for read buffer. Will be freed later. */
	if((buff = malloc(BUFF_SIZE)) == NULL) {
		return 1;
	}

	newpath = NULL;

	/*
	 * Check permissions of source for use with destination.
	 * Also check if source is a directory.
	 */
	if (stat(src, &stat_buff) == 0) {
		perms = stat_buff.st_mode;
		inode = stat_buff.st_ino;
	} else {
		return 1;
		/* NOTREACHED */
	}

	if (S_ISDIR(stat_buff.st_mode)) {
		return 3;
		/* NOTREACHED */
	}

	if ((srcfd = open(src, O_RDONLY)) < 0) {
		return 1;
		/* NOTREACHED */
	}

	/*
	 * Stat destination file. If it does not exist, do not return.
	 * Just create the file later.
	 */
	if(stat(dst, &stat_buff) < 0 && errno != ENOENT) {
		return 2;
		/* NOTREACHED */
	}

	/* Check if destination is a directory. */
	if (S_ISDIR(stat_buff.st_mode)) {
		/*
		 * If dst is a directory, find the source filename to be
		 * appended to the dst path. Allocated newpath will be
		 * freed later.
		 */
		if ((tmp = strrchr(src, '/')) == NULL)
			tmp = src;

		if (dst[strlen(dst)-1] != '/') {
			if ((newpath = malloc(strlen(tmp)+strlen(dst)+1))
				 == NULL) {
				return 2;
				/* NOTREACHED */
			}

			strncat(newpath, dst, strlen(dst));
			strncat(newpath, "/", 1);

		} else {
			if ((newpath = malloc(strlen(tmp)+strlen(dst)))
				 == NULL) {
				return 2;
				/* NOTREACHED */
			}

			strncat(newpath, dst, strlen(dst));

		}
		strncat(newpath, tmp, strlen(tmp));
	}
	/* If it is not a directory, create the file later. */

	/* Check to see if we are copying a file to itself. */
	if (newpath != NULL) {
		if (stat(newpath, &stat_buff) < 0 && errno != ENOENT) {
			return 2;
			/* NOTREACHED */
		}
	} else {
		if (stat(dst, &stat_buff) < 0 && errno != ENOENT) {
			return 2;
			/* NOTREACHED */
		}
	}

	if (stat(dst, &stat_buff) == 0) {
		if (stat_buff.st_ino == inode) {
			return 4;
			/* NOTREACHED */
		}
	}

	/* Check to see if we made a new path, otherwise use the given one. */
	if (newpath != NULL)
		dstfd = open(newpath, O_CREAT | O_WRONLY | O_TRUNC, perms);
	else
		dstfd = open(dst, O_CREAT | O_WRONLY | O_TRUNC, perms);

	if (dstfd < 0) {
		close(srcfd);
		return 2;
		/* NOTREACHED */
	}

	while ((n = read(srcfd, buff, BUFF_SIZE)) > 0) {
		if (write(dstfd, buff, n) != n) {
			close(srcfd);
			close(dstfd);
			return 2;
			/* NOTREACHED */
		}
	}

	/* Check if read ended on error */
	if (n < 0) {
		close(srcfd);
		close(dstfd);
		return 1;
		/* NOTREACHED */
	}

	if (close(srcfd) < 0)
		return 1;
		/* NOTREACHED */
	if (close(dstfd) < 0)
		return 2;
		/* NOTREACHED */
	free(buff);

	/* Check to see if we need to free newpath. */
	if (newpath != NULL)
		free(newpath);

	return 0;
}

void usage()
{
	(void)fprintf(stderr, "usage: tcp [source] [destination]\n");
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}
