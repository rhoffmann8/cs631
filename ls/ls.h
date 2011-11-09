/*
 * ls.h - Header file containing the options structure and traversal function
 * prototypes.
 */

#ifndef _LS_H_
#define _LS_H_

struct options {
	int show_hidden,		/* A */
	    show_all,			/* a */
	    sort_changed,		/* c */
	    dir_nocontents,		/* d */
	    show_filesymbol,		/* f */
	    sort_none,			/* F */
	    human_readable,		/* h */
	    show_inode,			/* i */
	    size_kilobytes,		/* k */
	    long_format,		/* l */
	    long_numids,		/* n */
	    print_nonprintable,		/* q */
	    dir_recurse,		/* R */
	    sort_reverse,		/* r */
	    sort_size,			/* S */
	    show_blocks,		/* s */
	    sort_modified,		/* t */
	    sort_access,		/* u */
	    print_raw,			/* w */
	    separate_lines,		/* 1 */
	    print_headers;
};

void check_argument_files(FTS*);
void check_physical_files(FTS*);
int check_file_errors(FTSENT*);
int main(int, char*[]);

#endif
