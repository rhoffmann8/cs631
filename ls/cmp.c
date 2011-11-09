/*
 * cmp.c - Module containing sorting functions to be used when retrieving
 * file heirarchies via fts_open.
 *
 * Notice that sort_reverse flips the result. One may believe that in the
 * case of a non-lexicographical tie the resulting lexicographical sort
 * would also be flipped (which is incorrect), but the "hack" here is that
 * the result of the lexicographical sort would also be flipped by the
 * reverse flag, correcting the issue.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"
#include "ls.h"

extern int (*cmpfunc)(const FTSENT*, const FTSENT*);
extern struct options opts;

/*
 * Function to sort by file access time.
 */
int access_cmp(const FTSENT *a, const FTSENT *b) {

	int ret;

	if (a->fts_statp->st_atime > b->fts_statp->st_atime)
		ret = -1;
	else if (a->fts_statp->st_atime < b->fts_statp->st_atime)
		ret = 1;
	else
		ret = name_cmp(a, b);

	if (opts.sort_reverse)
		ret /= -1;
	return ret;
}

/*
 * Function to sort by file changed time.
 */
int changed_cmp(const FTSENT *a, const FTSENT *b) {

	int ret;

	if (a->fts_statp->st_ctime > b->fts_statp->st_ctime)
		ret = -1;
	else if (a->fts_statp->st_ctime < b->fts_statp->st_ctime)
		ret = 1;
	else
		ret = name_cmp(a, b);

	if (opts.sort_reverse)
		ret /= -1;
	return ret;
}

/*
 * Master compare function to call the appropriate sorting function.
 */
int compare(const FTSENT **a, const FTSENT **b) {
	return cmpfunc(*a, *b);
}

/*
 * Function to sort by file modified time.
 */
int modified_cmp(const FTSENT *a, const FTSENT *b) {

	int ret;

	if (a->fts_statp->st_mtime > b->fts_statp->st_mtime)
		ret = -1;
	else if (a->fts_statp->st_mtime < b->fts_statp->st_mtime)
		ret = 1;
	else
		ret = name_cmp(a, b);

	if (opts.sort_reverse)
		ret /= -1;
	return ret;
}

/*
 * Function to sort by lexicographical order.
 */
int name_cmp(const FTSENT *a, const FTSENT *b) {

	int ret;
	char *buf1, *buf2;

	if ((buf1 = malloc(strlen(a->fts_name)+1)) == NULL) {
		fprintf(stderr, "Memory allocation error\n");
		custom_errno = -1;
	}

	if ((buf2 = malloc(strlen(b->fts_name)+1)) == NULL) {
		fprintf(stderr, "Memory allocation error\n");
		custom_errno = -1;
	}

	if (strncpy(buf1, a->fts_name, strlen(a->fts_name)+1) == NULL) {
		fprintf(stderr, "strncpy error\n");
		custom_errno = -1;
	}

        if (strncpy(buf2, b->fts_name, strlen(b->fts_name)+1) == NULL) {
		fprintf(stderr, "strncpy error\n");
		custom_errno = -1;
	}

	ret = cmp_lower_case(buf1, buf2);
	if (opts.sort_reverse)
		ret /= -1;

	free(buf1);
	free(buf2);

	return ret;
}

/*
 * Function to sort by file size.
 */
int size_cmp(const FTSENT *a, const FTSENT *b) {

	int ret;

	if (a->fts_statp->st_size > b->fts_statp->st_size)
		ret = -1;
	else if (a->fts_statp->st_size < b->fts_statp->st_size)
		ret = 1;
	else
		ret = name_cmp(a, b);

	if (opts.sort_reverse)
		ret /= -1;
	return ret;
}

/*
 * Function that takes two strings, converts them to lowercase, and
 * returns their comparison. To be used in namecmp, since ls output looks
 * nicer if case sensitivity does not matter.
 */
int cmp_lower_case(char* str1, char* str2) {

	int i;

	for(i = 0; i < strlen(str1); i++)
		str1[i] = tolower(str1[i]);
	for(i = 0; i < strlen(str2); i++)
		str2[i] = tolower(str2[i]);

	return strcmp(str1, str2);
}
