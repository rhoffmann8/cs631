/*
 * util.c - Module with utility and misc functions.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <bsd/stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "extern.h"
#include "ls.h"
#include "print.h"
#include "util.h"

/*
 * Function to determine how wide columns for certain fields should be.
 */
int check_max_widths(struct max_widths *m_widths, struct print_info *pinfo) {

	size_t file_size;
	char sizebuf[5];
	int block_size;
	int templen;
	int tmp_size;

	file_size = pinfo->statp->st_size;

	/* Blocks column */
	if (opts.show_blocks) {
		if (getenv("BLOCKSIZE") != NULL)
			block_size = atoi(getenv("BLOCKSIZE"));
		else
			block_size = 512;

		tmp_size = (pinfo->statp->st_blocks*512);
                        if (tmp_size / block_size > 0)
                                tmp_size = (tmp_size / block_size) + 1;
			else
				tmp_size = (tmp_size / block_size);

		if (opts.human_readable) {
			if ((templen = (humanize_number(sizebuf,
				sizeof(sizebuf),
				(unsigned long)pinfo->statp->st_blocks*512,
				"", HN_AUTOSCALE,
				(HN_DECIMAL | HN_NOSPACE)))) < 0) {
				fprintf(stderr, "Error in humanize_number\n");
				if (num_digits(tmp_size) > m_widths->blksize) {
					m_widths->blksize = num_digits(tmp_size);
				}
			} else {
				if (templen > m_widths->blksize) {
					m_widths->blksize = templen;
				}
			}
		} else {
			if (num_digits(tmp_size) > m_widths->blksize) {
				m_widths->blksize = num_digits(tmp_size);
			}
		}
	}

	/* UID/GID column */
	if (!opts.long_numids) {

		/* Error check */
		if (getpwuid(pinfo->statp->st_uid) == NULL) {
			/* Error, errno already set */
			strncpy(error_buff, "UID Error", 10);
			return -1;
		}

		if (getgrgid(pinfo->statp->st_gid) == NULL) {
			/* Error, errno already set */
			strncpy(error_buff, "GID Error", 10);
			return -1;
		}

		if (strlen(getpwuid(pinfo->statp->st_uid)->pw_name)
			> m_widths->uid) {
			m_widths->uid = strlen(getpwuid(
					pinfo->statp->st_uid)->pw_name);
		}

		if (strlen(getgrgid(pinfo->statp->st_gid)->gr_name)
			> m_widths->gid) {
			m_widths->gid = strlen(getgrgid(
					pinfo->statp->st_gid)->gr_name);
		}
	} else {
		if (num_digits(pinfo->statp->st_uid) > m_widths->uid) {
			m_widths->uid = num_digits(pinfo->statp->st_uid);
		}
		if (num_digits(pinfo->statp->st_gid) > m_widths->gid) {
			m_widths->gid = num_digits(pinfo->statp->st_gid);
		}
	}

	/* Inode column */
	if (opts.show_inode) {
		if (num_digits(pinfo->statp->st_ino) > m_widths->inode)
			m_widths->inode = num_digits(pinfo->statp->st_ino);
	}

	/* File size column */
	if (opts.size_kilobytes) {
		if ((file_size % 1024) > 0)
			file_size = (file_size / 1024) + 1;
		else
			file_size = file_size / 1024;
	}

	/* If character special or block device, we only care about rdev */
	if (S_ISCHR(pinfo->statp->st_mode) || S_ISBLK(pinfo->statp->st_mode)) {

		if (num_digits(major(pinfo->statp->st_rdev))
			> m_widths->major) {
			m_widths->major = num_digits(
				major(pinfo->statp->st_rdev));
		}
		/* Minor ID and size share the same column, so just set size */
		if (num_digits(minor(pinfo->statp->st_rdev))
			> m_widths->size) {
			m_widths->size = num_digits(
				minor(pinfo->statp->st_rdev));
		}
	}
	/* Otherwise, look at the file size */
	else if (opts.human_readable) {
		if ((templen = (humanize_number(sizebuf, sizeof(sizebuf),
			file_size, "", HN_AUTOSCALE,
			(HN_DECIMAL | HN_NOSPACE)))) < 0) {
			fprintf(stderr, "Error in humanize_number\n");
			/* If human_readable fails, use the number */
			if (num_digits(file_size) > m_widths->size)
				m_widths->size = num_digits(file_size);
		} else {
			if (templen > m_widths->size) {
				m_widths->size = templen;
			}
		}
	} else {
		if (num_digits(file_size) > m_widths->size)
			m_widths->size = num_digits(file_size);
	}

	return 0;
}

/*
 * Function to calculate the number of digits in a given number.
 */
int num_digits(unsigned long num) {
	if (num < 10)
		return 1;
	return (1 + num_digits(num/10));
}
