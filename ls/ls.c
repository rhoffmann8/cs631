/*
 * Rob Hoffmann, CS631
 * ls - List contents of directory
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
#include "print.h"
#include "ls.h"

int list(int, char *[], struct options);
static void usage(void);
int main(int, char *[]);

int (*cmpfunc)(const FTSENT*, const FTSENT*);
void (*printfunc)(FTS*);

char error_buff[1024];

/*
 * Program to output a list of files based on given parameters. Handles
 * flag parsing and setting in the main function, then wraps all options
 * into a struct and passes that along with a file tree structure to the
 * list function, which handles retrieval and sorting of files. Actual
 * output is handled by a printing function from print.c.
 */
int
main(int argc, char *argv[]) {
	char c;

	/* Default compare func to name, print func to non-long */
	cmpfunc = name_cmp;

	/* Set flag defaults */
	opts.separate_lines = 1;

	/* Check for output to a terminal */
	if (isatty(fileno(stdout)))
		opts.print_nonprintable = 1;
	else
		opts.print_raw = 1;

	/* Check for admin user */
	if (getuid() == 0)
		opts.show_hidden = 1;

	/* Parse flags */
	while ((c = getopt(argc, argv, "AacdFfhiklnqRrSstuw1")) != -1) {
		switch (c) {
		case 'A':
			opts.show_hidden = 1;
			break;
		case 'a':
			opts.show_all = 1;
			opts.show_hidden = 0;
			break;
		case 'c':
			/* -t must have been present */
			if (opts.sort_modified == 1) {
				opts.sort_changed = 1;
				opts.sort_access = 0;
				cmpfunc = changed_cmp;
			}
			break;
		case 'd':
			opts.dir_nocontents = 1;
			break;
		case 'F':
			opts.show_filesymbol = 1;
			break;
		case 'f':
			opts.sort_none = 1;
			break;
		case 'h':
			opts.human_readable = 1;
			opts.size_kilobytes = 0;
			break;
		case 'i':
			opts.show_inode = 1;
			break;
		case 'k':
			opts.size_kilobytes = 1;
			opts.human_readable = 0;
			break;
		case 'l':
			opts.separate_lines = 0;
			opts.long_format = 1;
			break;
		case 'n':
			opts.long_numids = 1;
			break;
		case 'q':
			opts.print_nonprintable = 1;
			opts.print_raw = 0;
			break;
		case 'R':
			opts.dir_recurse = 1;
			opts.print_headers = 1;
			break;
		case 'r':
			opts.sort_reverse = 1;
			break;
		case 'S':
			opts.sort_size = 1;
			cmpfunc = size_cmp;
			break;
		case 's':
			opts.show_blocks = 1;
			break;
		case 't':
			if (opts.sort_modified == 0) {
				opts.sort_modified = 1;
				cmpfunc = modified_cmp;
			}
			break;
		case 'u':
			/* -t must have been present */
			if (opts.sort_modified == 1) {
				opts.sort_access = 1;
				opts.sort_changed = 0;
				cmpfunc = access_cmp;
			}
			break;
		case 'w':
			opts.print_raw = 1;
			opts.print_nonprintable = 0;
			break;
		case '1':
			opts.separate_lines = 1;
			opts.long_numids = 0;
			opts.long_format = 0;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (list(argc, argv, opts) < 0) {
		if (custom_errno < 0) {
			/* Error was already printed, just exit */
			exit(EXIT_FAILURE);
		} else {
			/*
			 * Add null terminator just in case an error was
			 * too long.
			 */
			error_buff[1023] = '\0';
			/*
			 * Print out the error in error_buff along with the
			 * errno string.
			 */
			fprintf(stderr, "%s: %s\n",
				error_buff, strerror(errno));
		}
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

/*
 * List function. Determines the options for fts_open and calls it.
 * Passes resulting file heirarchy pointer to print function.
 */
int list(int argc, char* argv[], struct options opts) {

	FTS *ftsp;
	int fts_options;
	char *cwd[] = {".", NULL};
	char **path = NULL;

	/* Check if path(s) provided */
	if (argc == 0) {
		path = cwd;
		argc = 1;
	} else {
		path = argv;
	}

	/* Figure out fts options from flags */
	fts_options = 0;
	if (!opts.show_filesymbol && !opts.dir_nocontents && !opts.long_format)
		fts_options |= FTS_COMFOLLOW;
	if (opts.show_all)
		fts_options |= FTS_SEEDOT;

	fts_options |= FTS_PHYSICAL;
	fts_options |= FTS_NOCHDIR;

	/* Grab file heirarchy using specified sort function */
	if ((ftsp = fts_open(path, fts_options, opts.sort_none ? NULL :
		compare)) == NULL) {
		return -1;
	}

	/* Check for errors, keep going if just the case of no such file */
	if ((errno > 0 && errno != ENOENT) || custom_errno < 0) {
		return -1;
	}

	check_argument_files(ftsp);
	if (!opts.dir_nocontents)
		check_physical_files(ftsp);

	/* Check for errors from traversing and printing */
	if(errno > 0 || custom_errno < 0)
		return -1;

	/* Close heirarchy */
	if(fts_close(ftsp) < 0) {
		return -1;
	}

	return 0;
}

/*
 * Function to check for errors in a FTSENT.
 */
int check_file_errors(FTSENT* file) {
	switch(file->fts_info) {
	case FTS_DC:
		fprintf(stderr, "Warning: %s: %s\n",
		file->fts_name, strerror(file->fts_errno));
		return 0;
	case FTS_DNR:
	case FTS_ERR:
		errno = file->fts_errno;
		strncpy(error_buff, file->fts_name,
			sizeof(error_buff));
		return -1;
	}

	return 0;
}


/*
 * Function to traverse the files in the arguments given by the user.
 * This function uses fts_children to check each file instead of
 * fts_read, which is used later. This will only print actual files
 * that were provided -- directories are deferred until later.
 */
void check_argument_files(FTS *ftsp) {

	FTSENT *child;
	struct print_info *pinfo_head, *pinfo_cur, *pinfo_tmp;
	int dir_count;

	files_printed = 0;
	dir_count = 0;
	child = fts_children(ftsp, 0);
	pinfo_head = pinfo_cur = pinfo_tmp = NULL;

	/*
	 * First we need to iterate through the children and see if
	 * multiple directories were provided as arguments. If so,
	 * we will need to print headers.
	 */
	if (!opts.dir_recurse) {
		while (child != NULL) {
			if (check_file_errors(child) < 0)
				return;
			if (child->fts_info == FTS_D) {
				dir_count++;
				if (dir_count > 1) {
					opts.print_headers = 1;
					break;
				}
			}
			child = child->fts_link;
		}
	}

	/* Reset to first child of arguments */
	child = fts_children(ftsp, 0);

	while (child != NULL && custom_errno == 0) {

		/* Do errno check here so we can exclude certain ones */
		if (errno == EACCES) {
			fprintf(stderr, "Permission denied\n");
			errno = 0;
			continue;
		} else if (errno > 0) {
			fprintf(stderr, "%d\n", errno);
			return;
		}

		/* Error check */
		if (check_file_errors(child) < 0)
			return;

		/*
		 * If we can't stat the file, most likely it doesn't
		 * exist. We print an error, but there's no reason we
		 * can't continue on to the other files.
		 */
		if (child->fts_info == FTS_NS) {
			fprintf(stderr,
				"%s: No such file or couldn't stat\n",
				child->fts_name);
			opts.print_headers = 1;
			child = child->fts_link;
			continue;
		}

		/*
		 * If argument is a directory, save it for later, after we
		 * output all files first.
		 */
		if (child->fts_info ==  FTS_D) {
			if (!opts.dir_nocontents) {
				child = child->fts_link;
				continue;
			}
		}

		/* Check hidden file flags */
		if (!opts.show_all && !opts.show_hidden) {
			if (child->fts_name[0] == '.') {
				child = child->fts_link;
				continue;
			}
		}

		if (!opts.show_all && opts.show_hidden) {
			if (strcmp(child->fts_name, ".") == 0 ||
				strcmp(child->fts_name, "..") == 0) {
				continue;
			}
		}

		/* Insert this file into print_info linked list */
		if ((pinfo_tmp = malloc(sizeof(struct print_info))) == NULL) {
			fprintf(stderr, "Memory allocation error");
			custom_errno = -1;
			return;
		}

		/* Set print_info node properties */
		pinfo_tmp->parent_path = NULL;
		pinfo_tmp->name = child->fts_name;
		pinfo_tmp->path = child->fts_path;
		pinfo_tmp->statp = child->fts_statp;

		if (pinfo_head == NULL)
			pinfo_head = pinfo_cur = pinfo_tmp;
		else {
			pinfo_cur->next = pinfo_tmp;
			pinfo_cur = pinfo_cur->next;
		}

		child = child->fts_link;
	}

	/* So we don't segfault -- uninitialized != NULL */
	if (pinfo_cur != NULL)
		pinfo_cur->next = NULL;

	/* Print out LL of print_info if we put any files in there */
	if (pinfo_head != NULL) {
		if (opts.long_format || opts.long_numids)
			print_long(pinfo_head);
		else
			print(pinfo_head);
		files_printed++;
	}

	/* When we're done printing files, free the LL */
	pinfo_cur = pinfo_head;
	while (pinfo_cur != NULL) {
		pinfo_cur = pinfo_head->next;
		free(pinfo_head);
		pinfo_head = pinfo_cur;
	}

	/* If d flag is set, we're done here. Return and close ftsp. */
	if (opts.dir_nocontents)
		return;

	/*
	 * If we printed files from the argument list, we're going to
	 * need headers for the directories.
	 */
	if (files_printed > 0)
		opts.print_headers = 1;
}

/*
 * Function to traverse the files in the directories provided by
 * the user. It does this by using fts_read on the logical file
 * heirarcy provided and ignoring the files (as we already printed
 * any before).
 */
void check_physical_files(FTS *ftsp) {

	FTSENT *file, *child;
	struct print_info *pinfo_head, *pinfo_cur, *pinfo_tmp;
	int num_children = 0;
	int newline_between_dirs = 0;

	pinfo_head = pinfo_cur = pinfo_tmp = NULL;

	/* Begin reading files from the heirarchy */
	while ((file = fts_read(ftsp)) != NULL && custom_errno == 0) {

		/* Do errno check here, so we can exclude certain cases */
		if (errno == EACCES) {
			/*
			 * If permission denied, show the error but continue
			 * program execution.
			 */
			fprintf(stderr, "Permission denied\n");
			errno = 0;
			continue;
		} else if (errno > 0)
			return;

		/* Error check */
		if (check_file_errors(file) < 0)
			return;

		/* Anything that isn't a directory was already printed */
		if (file->fts_info == FTS_D) {

			/*
			 * If we printed any files during argument file
			 * traversal, print a newline. Don't do this again.
			 */
			if (files_printed > 0) {
				printf("\n");
				files_printed--;
			}

			/*
			 * If we encounter a hidden (.) file and neither
			 * -A or -a are set, skip it.
			 */
			if (file->fts_level != 0 && file->fts_name[0] == '.'
				&& !opts.show_hidden && !opts.show_all)
				continue;

			/*
			 * As the variable name suggests, we need to print
			 * a newline between directory listings. Don't do
			 * this the first time we output a dir, but do it
			 * for subsequent dirs.
			 */
			if (newline_between_dirs > 0)
				printf("\n");
			newline_between_dirs++;

			/*
			 * Go through the children of the directory currently
			 * being read.
			 */
			child = fts_children(ftsp, 0);
			while (child != NULL) {
				/*
				 * Keep tally of children. Since print
				 * functions take care of printing headers
				 * and will not be called if there are no
				 * children, use this flag to tell this
				 * function to print the header instead.
				 */
				num_children++;

				/* Error check */
				if (check_file_errors(child) < 0)
					return;

				/* If not -a but -A show . file, else don't */
				if (!opts.show_all) {
					if (child->fts_name[0] == '.' &&
						!opts.show_hidden) {
						child = child->fts_link;
						continue;
					}
				}
				/* Insert this file into print_info LL */
				if ((pinfo_tmp = malloc(
					sizeof(struct print_info))) == NULL) {
					fprintf(stderr,
						"Memory allocation error");
					custom_errno = -1;
					return;
				}

				/* Set print_info properties */
				pinfo_tmp->parent_path = file->fts_path;
				pinfo_tmp->name = child->fts_name;
				pinfo_tmp->path = child->fts_path;
				pinfo_tmp->statp = child->fts_statp;

				if (pinfo_head == NULL)
					pinfo_head = pinfo_cur = pinfo_tmp;
				else {
					pinfo_cur->next = pinfo_tmp;
					pinfo_cur = pinfo_cur->next;
				}

				child = child->fts_link;
			}

			/* Print the header here if no children found */
			if (num_children == 0) {
				printf("%s:\n", file->fts_path);
				if (opts.show_blocks)
					printf("total 0\n");
			}

			/* So we don't segfault -- uninitialized != NULL */
			if (pinfo_cur != NULL)
				pinfo_cur->next = NULL;

			/* Print print_info LL based on options */
			if (pinfo_head != NULL) {
				if (opts.long_format || opts.long_numids)
					print_long(pinfo_head);
				else
					print(pinfo_head);
			}

			/* If we're not recursing, don't go into subdirs */
			if (!opts.dir_recurse)
				fts_set(ftsp, file, FTS_SKIP);

			/* When we're done with this directory, free the LL */
			pinfo_cur = pinfo_head;
			while (pinfo_cur != NULL) {
				pinfo_cur = pinfo_head->next;
				free(pinfo_head);
				pinfo_head = pinfo_cur;
			}
			pinfo_head = pinfo_cur = NULL;

			/* Reset children counter */
			num_children = 0;
		}
	}
}

static void
usage(void) {
	fprintf(stderr,
		"usage: ls [-AacdFfhiklnqRrSstuw1] [file...]\n");
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}
