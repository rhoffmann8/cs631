/*
 * extern.h - Header file containing functions and golbals used by multiple
 * source files.
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

int access_cmp(const FTSENT*, const FTSENT*);
int changed_cmp(const FTSENT*, const FTSENT*);
int compare(const FTSENT**, const FTSENT**);
int modified_cmp(const FTSENT*, const FTSENT*);
int name_cmp(const FTSENT*, const FTSENT*);
int size_cmp(const FTSENT*, const FTSENT*);
int cmp_lower_case(char*, char*);
int (*cmpfunc)(const FTSENT*, const FTSENT*);

char error_buff[1024];
struct options opts;

/*
 * Since errno is not set for errors such as NULL returns on mallocs, have
 * an errno which is set to a negative number for those cases. On-the-fly
 * error printing will be done in conjunction with assignment to this var,
 * but it will signify that the function is was in did fail and force the
 * program to return EXIT_FAILURE.
 */
int custom_errno;

/* Simple flag used for tracking newlines */
int files_printed;

#endif
