#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Function to check if a file.dir is in a given directory.
 * Argument "type" is 0 to look for file, 1 for directory.
 * Returns 1 if found, 0 if not found, and -1 on error.
 */
int
file_in_dir(DIR *dp, ino_t root_ino, ino_t needle_ino, char *dirname, int type) {

	DIR *dp2;
	struct dirent *dir;
	struct stat stat_buf;
	int ret;
	char tmp[PATH_MAX];

	while ((dir = readdir(dp)) != NULL) {
		if (strcmp(dir->d_name, ".") != 0 &&
			strcmp(dir->d_name, "..") != 0) {

			bzero(tmp, sizeof(tmp));

			strncpy(tmp, dirname, strlen(dirname));
			strncat(tmp, "/", 1);
			strncat(tmp, dir->d_name, strlen(dir->d_name));

			if (stat(tmp, &stat_buf) < 0) {
				perror("stat");
				return -1;
			}

			if (type == 1 && S_ISDIR(stat_buf.st_mode)) {
				if (dir->d_ino == needle_ino)
					return 1;
				if (dir->d_ino == root_ino)
					return 0;
				if ((dp2 = opendir(tmp)) < 0) {
					perror("opendir");
					return -1;
				}
				ret = file_in_dir(dp2, root_ino, needle_ino,
					tmp, type);
				if (closedir(dp2) < 0) {
					perror("closedir");
					return -1;
				}
				if (ret == 1 || ret == -1)
					return ret;
			} else if (type == 0 && S_ISREG(stat_buf.st_mode)) {
				if (dir->d_ino == needle_ino)
					return 1;
			}
		}
	}

	return 0;
}

/*
 * Function to check if a file is inside a root path.
 * Returns 1 if it is, else returns 0.
 */
int
file_in_root(char *path) {

	int i;
	char *tmp;

	i = 0;
	tmp = path;
	/* If request is a user's dir, treat all text after ~<user>/ as the path */
	if (path[1] == '~') {
		tmp++;
		if (strchr(tmp, '/') == NULL)
			i++;
		else
			while (*tmp != '/') tmp++;
	}

	while (*tmp != '\0') {
		/*
		 * If we find a slash (that is not part of
		 * multiple slashes), increment i
		 */
		if ((*tmp == '/') && (*(tmp-1) != '/')) {
			i++; tmp++;
		}

		/*
		 * If we find a parent dir traversal sequence,
		 * decrement i
		 */
		else if (strncmp(tmp, "../", 3) == 0) {
			i--; tmp += 3;
		}
		/* For all other characters, do nothing */
		else {
			tmp++;
		}

		/* If we leave the root dir at any point, send 403 */
		if (i < 1)
			return 0;
	}

	return 1;
}

/*
 * Returns file path relative to the user-specified root,
 * or an absolute path to a /home/<user>/sws/ directory if
 * request began with '~'.
 */
char*
sws_file_path(char *root, char *req_path) {

	struct stat stat_buf;
	int i;
	char *tmp;
	char *tmp_path;

	if ((tmp_path = malloc(PATH_MAX)) == NULL) {
		fprintf(stderr, "malloc\n");
		return NULL;
	}

	i = 0;
	tmp = req_path;
	if (req_path[1] == '~') {
		tmp += 2;
		if (strchr(tmp, '/') != NULL)
			for (i = 0; *tmp != '/'; i++, tmp++);
		else
			i = strlen(tmp);
	}

	if (req_path[1] == '~') {
		tmp = req_path + 2;
		strncpy(tmp_path, "/home/", 6);
		strncat(tmp_path, tmp, i);
		for (; i > 0; tmp++, i--);
		strncat(tmp_path, "/sws/", 5);
		strncat(tmp_path, tmp+1, strlen(tmp));
	} else {
		strncpy(tmp_path, root, strlen(root));
		tmp_path[strlen(root)] = '\0';
		strncat(tmp_path, req_path, strlen(req_path));
	}

	if (stat(tmp_path, &stat_buf) < 0) {
		perror("utils.c: stat");
		return NULL;
	}

	if (S_ISDIR(stat_buf.st_mode)) {
		if (tmp_path[strlen(tmp_path)-1] != '/')
			strncat(tmp_path, "/", 1);
	} else {
		if (tmp_path[strlen(tmp_path)-1] == '/')
			tmp_path[strlen(tmp_path)-1] = '\0';
	}

	return tmp_path;
}

/*
 * Wrapper for strrchr which returns the numerical position
 * of the character found in the string.
 */
int
strrchr_pos(char *str, char c, int len) {

	int i, n;
	char *ptr, *ptr2;
	char buf[1024];

	bzero(buf, sizeof(buf));

	for (i = 0; i < len; i++)
		buf[i] = str[i];

	if ((ptr2 = strrchr(buf, c)) == NULL)
		return -1;
	ptr = buf;
	for (n = 0; n < len && ptr != ptr2; n++, ptr++);

	if (ptr != ptr2)
		return -1;

	return n;
}
