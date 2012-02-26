#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
strchr_pos(char *str, char c) {

	char *ptr;

	if ((ptr = strchr(str, c)) == NULL)
		return -1;
	return (int)(ptr - str);
}

int
strrchr_pos(char *str, char c) {

	char *ptr;

	if ((ptr = strrchr(str, c)) == NULL)
		return -1;
	return (int)(ptr - str);
}

void
concat(char *str, int n, ...) {

	va_list args;
	int i;
	char *tmp;

	va_start(args, n);

	for (i = 0; i < n; i++) {
		tmp = va_arg(args, char*);
		strncat(str, tmp, strlen(tmp));
	}
	va_end(args);
}

void
conncat(char *str, int n, ...) {

	va_list args;
	int i, len;
	char *tmp;

	va_start(args, n);

	for (i = 0; i < n; i++) {
		tmp = va_arg(args, char*);
		len = va_arg(args, int);
		strncat(str, tmp, len);
	}
	va_end(args);
}

char*
my_realpath(char *path) {

	int alloc_size, abs;
	char *ret, *tmp;
	char cwd[PATH_MAX];

	if (strlen(path) == 0)
		return NULL;

	/* Check for absolute path */
	if (path[0] != '/') {
		abs = 0;
		getcwd(cwd, PATH_MAX);
		alloc_size = strlen(path) + strlen(cwd) + 1;
	} else {
		abs = 1;
		alloc_size = strlen(path) + 1;
		path++;
	}

	/*
	 * The resulting path length must be less than or equal to the supplied
	 * path length (or in relative path case, that plus the length of the
	 * cwd)
	 */
	if ((ret = calloc(1, alloc_size)) == NULL) {
		fprintf(stderr, "calloc error\n");
		return NULL;
	}

	/* If not absolute, append cwd */
	if (!abs)
		strncpy(ret, cwd, strlen(cwd));
	else
		strncat(ret, "/", 1);

	while (*path != '\0') {
		if (*path == '/') {
			path++;
		} else if (*path == '.'
			&& (path[1] == '/' || path[1] == '\0')) {
			path++;
		} else if ((*path == '.' && *(path+1) == '.')
			&& (path[2] == '/' || path[2] == '\0')) {
			if ((tmp = strrchr(ret, '/')) != NULL) {
				if (tmp == strchr(ret, '/'))
					*(tmp+1) = '\0';
				else
					*(tmp) = '\0';
			} else {
				//we should never end up here, congrats if you do
				fprintf(stderr, "my_realpath: Unknown error\n");
				free(ret);
				return NULL;
			}
			path += 2;
		} else {
			if (ret[strlen(ret)-1] != '/')
				strncat(ret, "/", 1);
			for (;*path != '/' && *path != '\0'; path++)
				strncat(ret, path, 1);
		}
  	}
	//if (strlen(ret) == 0)
	//	strncpy(ret, "/", 1);

	//If we left a trailing '/' and it's not the whole path, remove it
	if (ret[strlen(ret)-1] == '/' && strlen(ret) > 1)
		ret[strlen(ret)-1] = '\0';

	//Resize allocated memory to actual string length
	if ((ret = realloc(ret, strlen(ret)+1)) == NULL) {
		fprintf(stderr, "realloc error\n");
		return NULL;
	}

	return ret;
}
