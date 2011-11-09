/*
 * print.c - File with functions to handle perparation of and actual printing
 * of sequences of files.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <bsd/stdlib.h>
#include <bsd/string.h>
#include <ctype.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"
#include "ls.h"
#include "print.h"
#include "util.h"

/*
 * Function to return appropriate character for F flag.
 */
char get_F_symbol(struct stat *statp) {
	mode_t mode;

	mode = statp->st_mode;

	if (S_ISDIR(mode))
		return '/';
	else if (S_ISREG(mode) && (mode & 0111))
		return '*';
	else if (S_ISLNK(mode))
		return '@';
	#ifdef S_IFWHT /* Not defined under Linux */
	else if ((mode & S_IFMT) == S_IFWHT)
		printf("%%");
	#endif
	else if (S_ISSOCK(mode))
		return '=';
	else if (S_ISFIFO(mode))
		return '|';

	return -1;
}

/*
 * Print function. Takes a linked list of print_info nodes and outputs
 * information about each node based on its properties.
 */
void print(struct print_info *pinfo_head) {

	struct max_widths *m_widths;
	struct print_info *pinfo_tmp;
	unsigned long block_size;
	unsigned long tmp_size;
	unsigned long total_size;
	char sizebuf[5];
	char *buf;
	char symbol;

	if ((m_widths = malloc(sizeof(struct max_widths))) == NULL) {
		fprintf(stderr, "Memory allocation error\n");
		custom_errno = -1;
		return;
	}

	/* Initialize max column widths */
	m_widths->blksize = m_widths->gid = m_widths->inode = m_widths->size
		= m_widths->name = m_widths->size = m_widths->uid
		= m_widths->major = m_widths->minor = 0;

	pinfo_tmp = pinfo_head;

	/* If we need headers, print headers */
	if (opts.print_headers && pinfo_head->parent_path != NULL)
		printf("%s:\n", pinfo_head->parent_path);

	total_size = 0;

	/* If -k, default to block_size 1024 */
	if (opts.size_kilobytes)
		block_size = 1024;
	/* Else get block environment variable or default to 512 */
	else if (getenv("BLOCKSIZE") != NULL)
		block_size = atoi(getenv("BLOCKSIZE"));
	else
		block_size = 512;

	/*
	 * Loop through files and calculate the max width of
	 * each column. While doing this, get the total size
	 * to be used for the "total" header later.
	 */
	while (pinfo_tmp != NULL) {
		if (check_max_widths(m_widths, pinfo_tmp) < 0)
			return;
		total_size += pinfo_tmp->statp->st_blocks;
		pinfo_tmp = pinfo_tmp->next;
	}

	if (((total_size*512) % block_size) > 0)
		total_size = ((total_size*512)/block_size) + 1;
	else
		total_size = (total_size*512)/block_size;

	/* If -s, and not -d and to a terminal, print the "total" header. */
	if (opts.show_blocks && !opts.dir_nocontents
		&& isatty(fileno(stdout))) {
		if (opts.human_readable) {
			if ((humanize_number(sizebuf, sizeof(sizebuf),
				total_size*block_size, "", HN_AUTOSCALE,
				(HN_DECIMAL | HN_NOSPACE))) < 0) {
				fprintf(stderr, "Error in humanize_number\n");
				printf("total %lu", total_size);
			} else {
				printf("total %s", sizebuf);
			}
			printf("\n");
		} else {
			printf("total %lu", total_size);
			printf("\n");
		}
	}

	/* Reset the LL pointer */
	pinfo_tmp = pinfo_head;

	while (pinfo_tmp != NULL) {
		/* Inode column */
		if (opts.show_inode) {
		printf("%*lu", m_widths->inode,
			(unsigned long)pinfo_tmp->statp->st_ino);
			printf(" ");
		}

		/* Blocks column */
		if (opts.show_blocks) {
			tmp_size = (pinfo_tmp->statp->st_blocks*512);
			if (tmp_size % block_size > 0)
				tmp_size = (tmp_size / block_size) + 1;
			else
				tmp_size = (tmp_size / block_size);

			if (opts.human_readable) {
				if ((humanize_number(sizebuf, sizeof(sizebuf),
					(unsigned long)pinfo_tmp->statp->st_blocks*512,
					"", HN_AUTOSCALE,
					(HN_DECIMAL | HN_NOSPACE))) < 0) {
					fprintf(stderr,
						"Error in humanize_number\n");
					printf("%*lu",
						m_widths->blksize, tmp_size);
				} else {
					printf("%*s", m_widths->blksize,
						sizebuf);
				}
			} else {
				printf("%*lu", m_widths->blksize, tmp_size);
			}
			printf(" ");
		}

		/* Check for q and print appropriately */
		if (opts.print_nonprintable) {
			if ((buf = malloc(strlen(pinfo_tmp->name)+1)) == NULL) {
				fprintf(stderr, "Memory allocation error\n");
				custom_errno = -1;
				return;
			}

			strncpy(buf, pinfo_tmp->name,
				strlen(pinfo_tmp->name)+1);
			buf = print_the_unprintable(buf);

			/* Print the file with '?' characters, if any */
			printf("%s", buf);

			free(buf);
		} else {
			/* Print raw filename */
			printf("%s", pinfo_tmp->name);
		}

		/* Check for -F */
		if (opts.show_filesymbol) {
			if ((symbol = get_F_symbol(pinfo_tmp->statp)) != -1)
				printf("%c", get_F_symbol(pinfo_tmp->statp));
		}

		/* Check for -1 */
		if (opts.separate_lines)
			printf("\n");

		pinfo_tmp = pinfo_tmp->next;
	}
}

/*
 * Print function to print a file in long (-l) format. Does the same as the
 * print function with additional information such as file mode, permissions,
 * owners, size, and last time modified.
 */
void print_long(struct print_info *pinfo_head) {

	struct print_info *pinfo_tmp;
	struct max_widths *m_widths;
	unsigned long block_size;
	unsigned long tmp_block_size;
	unsigned long total_size;
	unsigned long tmp_size;
	time_t list_time;
	size_t file_size;
	int bytes;
	char *buf;
	char date[13];
	char permissions[12];
	char sizebuf[5];
	char *tmpstring;
	char symbol;

	pinfo_tmp = pinfo_head;

	if ((m_widths = malloc(sizeof(struct max_widths))) == NULL) {
		fprintf(stderr, "Memory allocation error\n");
		custom_errno = -1;
		return;
	}

	/* Initialize max column widths */
	m_widths->blksize = m_widths->gid = m_widths->inode = m_widths->size
		= m_widths->name = m_widths->size = m_widths->uid
		= m_widths->major = m_widths->minor = 0;

	if (opts.print_headers && pinfo_head->parent_path != NULL)
		printf("%s:\n", pinfo_head->parent_path);

	total_size = 0;

	/* Same logic as print function */
	if (getenv("BLOCKSIZE") != NULL)
		block_size = atoi(getenv("BLOCKSIZE"));
	else
		block_size = 512;

	/* Same logic as print function */
	while (pinfo_tmp != NULL) {

		if (check_max_widths(m_widths, pinfo_tmp) < 0)
			return;

		total_size += pinfo_tmp->statp->st_blocks;
		pinfo_tmp = pinfo_tmp->next;
	}

	if (((total_size*512) % block_size) > 0)
		total_size = ((total_size*512)/block_size) + 1;
	else
		total_size = (total_size*512)/block_size;

	/* We print the total header by default if not -d, check for -h */
	if (!opts.dir_nocontents && isatty(fileno(stdout))) {
		if (opts.human_readable) {
			if ((humanize_number(sizebuf, sizeof(sizebuf),
				total_size*block_size, "", HN_AUTOSCALE,
				(HN_DECIMAL | HN_NOSPACE))) < 0) {
				fprintf(stderr, "Error in humanize_number\n");
				printf("total %lu", total_size);
			} else {
				printf("total %s", sizebuf);
			}
		} else {
			printf("total %lu", total_size);
		}
		printf("\n");
	}

	/* Reset LL pointer */
	pinfo_tmp = pinfo_head;

	while (pinfo_tmp != NULL && errno == 0 && custom_errno == 0) {

		/* Temp variable for shorter code */
		file_size = pinfo_tmp->statp->st_size;

		/* Inode column */
		if (opts.show_inode) {
			printf("%*lu", m_widths->inode,
				(unsigned long)pinfo_tmp->statp->st_ino);
			printf(" ");
		}

		/* Blocks column */
		if (opts.show_blocks) {

			/* If -k, set block_size to 1024 just for now */
			if (opts.size_kilobytes) {
				tmp_block_size = block_size;
				block_size = 1024;
			}

			tmp_size = (pinfo_tmp->statp->st_blocks*512);
			if ((tmp_size % block_size) > 0)
				tmp_size = (tmp_size / block_size) + 1;
			else
				tmp_size = (tmp_size / block_size);

			if (opts.human_readable) {
				if ((humanize_number(sizebuf, sizeof(sizebuf),
					(unsigned long)pinfo_tmp->statp->st_blocks*512,
					"", HN_AUTOSCALE,
					(HN_DECIMAL | HN_NOSPACE))) < 0) {
					fprintf(stderr,
						"Error in humanize_number\n");
					printf("%*lu",
						m_widths->blksize, tmp_size);
                                } else {
                                        printf("%*s",
						m_widths->blksize, sizebuf);
                                }
			} else {
				printf("%*lu", m_widths->blksize, tmp_size);
			}
			printf(" ");

			/* If -k, reset block size for calculations later */
			if (opts.size_kilobytes)
				block_size = tmp_block_size;
		}

		/* Print file mode and permissions */
		strmode(pinfo_tmp->statp->st_mode, permissions);
		printf("%s", permissions);

		/* Print number of hardlinks */
		printf("%lu", (unsigned long)pinfo_tmp->statp->st_nlink);
		printf(" ");

		/* Print uid/gid or names, depending on options */
		if (opts.long_numids) {
			printf("%*d", m_widths->uid, pinfo_tmp->statp->st_uid);
			printf(" ");
			printf("%*d", m_widths->gid, pinfo_tmp->statp->st_gid);
		} else {
			if (getpwuid(pinfo_tmp->statp->st_uid) == NULL) {
				/* Error, errno already set */
				strncpy(error_buff, "UID error",
					10);
				return;
			} else {
				printf("%*s", m_widths->uid, getpwuid(
					pinfo_tmp->statp->st_uid)->pw_name);
			}
			printf(" ");
			if (getgrgid(pinfo_tmp->statp->st_gid) == NULL) {
				/* Error, errno already set */
				strncpy(error_buff, "GID error",
					10);
				return;
			} else {
				printf("%*s", m_widths->gid, getgrgid(
					pinfo_tmp->statp->st_gid)->gr_name);
			}
		}
		printf(" ");

		/*
		 * If character special or block device, replace size with
		 * major and minor rdev IDs instead.
		 */
		if (S_ISCHR(pinfo_tmp->statp->st_mode) ||
			S_ISBLK(pinfo_tmp->statp->st_mode)) {

			printf("%*d,%*d", m_widths->major,
				major(pinfo_tmp->statp->st_rdev),
				m_widths->size,
				minor(pinfo_tmp->statp->st_rdev));
		} else {
			/* Print file size, in bytes or converted */
			if(opts.size_kilobytes) {
				if ((file_size % 1024) > 0)
					file_size = (file_size / 1024) + 1;
				else
					file_size = (file_size / 1024);
			}

			if (opts.human_readable) {
				if ((humanize_number(sizebuf, sizeof(sizebuf),
					pinfo_tmp->statp->st_size, "",
					HN_AUTOSCALE,
					(HN_DECIMAL | HN_NOSPACE))) < 0) {
					/*
					 * Print error, but no reason why we
					 * can't continue using the numerical
					 * size
					 */
					fprintf(stderr,
						"Error in humanize_number\n");
					printf("%*lu", m_widths->size,
						(unsigned long)file_size);
				} else {
					printf("%*s", m_widths->size, sizebuf);
				}
			} else {
				/*
				 * If we had printed block/char devices, use
				 * larger column size
				 */
				if (m_widths->major != 0) {
					printf("%*lu",
						m_widths->size +
							m_widths->major + 1,
						(unsigned long)file_size);
				} else {
					printf("%*lu", m_widths->size,
						(unsigned long)file_size);
				}
			}
		}
		printf(" ");

		/* Get time to display based on flags */
		if (opts.sort_access)
			list_time = pinfo_tmp->statp->st_atime;
		else if (opts.sort_changed)
			list_time = pinfo_tmp->statp->st_ctime;
		else
			list_time = pinfo_tmp->statp->st_mtime;

		/* Format time */
		if (strftime(date, 13, "%b %e %k:%M",
			localtime(&(list_time))) != 12) {
			fprintf(stderr,
				"Warning: strftime returned unexpected result\n");
		}

		/* Print date */
		printf("%s", date);
		printf(" ");

		/* Check for q and print appropriately */
		if (opts.print_nonprintable) {
			if ((buf = malloc(strlen(pinfo_tmp->name)+1)) == NULL) {
				fprintf(stderr, "Memory allocation error\n");
				custom_errno = -1;
				return;
			}

			strncpy(buf, pinfo_tmp->name,
				strlen(pinfo_tmp->name)+1);
			buf = print_the_unprintable(buf);

			/* Print the file with '?' characters, if any */
			printf("%s", buf);

			free(buf);
		} else {
			/* Print raw filename */
			printf("%s", pinfo_tmp->name);
		}

		/* If -F set, print the appropriate symbol */
		if (opts.show_filesymbol) {
			if ((symbol = get_F_symbol(pinfo_tmp->statp)) != -1)
				printf("%c", get_F_symbol(pinfo_tmp->statp));
		}

		/* Special case for links -- show linked file */
		if (S_ISLNK(pinfo_tmp->statp->st_mode)) {
			printf(" ");
			if ((tmpstring = malloc(
					strlen(pinfo_tmp->name)
					+strlen(pinfo_tmp->parent_path)+2))
					 == NULL) {
				fprintf(stderr, "Memory allocation error\n");
				custom_errno = -1;
				return;
			}

			/*
			 * Copy the path into a new string to be passsed to
			 * readlink.
			 */
			strncpy(tmpstring, pinfo_tmp->parent_path,
				strlen(pinfo_tmp->parent_path)+1);
			strncat(tmpstring, "/", 2);
			strncat(tmpstring, pinfo_tmp->name,
				strlen(pinfo_tmp->name));
			strncat(tmpstring, "\0", 1);

			if ((buf = malloc(file_size + 1)) == NULL) {
				fprintf(stderr, "Memory allocation error\n");
				custom_errno = -1;
				return;
			}
			if ((bytes = (readlink(tmpstring, buf, file_size+1)))
				< 0) {
				/* Output error, but proceed with execution */
				fprintf(stderr, "%s: Readlink error\n",
					tmpstring);
			} else {
				buf[bytes] = '\0';
				printf("-> %s", buf);
			}

			free(buf);
			free(tmpstring);
		}

		printf("\n");

		pinfo_tmp = pinfo_tmp->next;
	}
}

/*
 * Function to check for unprintable characters in a string and convert them
 * to the '?' character.
 */
char* print_the_unprintable(char* str) {

	int i;

	for (i = 0; i < strlen(str); i++) {
		if (!isprint(str[i]))
			str[i] = '?';
	}

	return str;
}
