/*
 * DirectoryUtil.c
 *
 *  Created on: 10-Mar-2015
 *      Author: Debashis Ganguly
 */

#include "../SFTP.h"

DArray *getAllFilesInCurrentDirectory() {
	DArray *fileList = DArray_create(sizeof(char) * PATH_MAX, 100);

	DIR *dir;
	struct dirent *ent;

	if ((dir = opendir(".")) < 0) {
		perror("Failed to open current directory while listing...\r\n");
		return NULL;
	}

	while ((ent = readdir(dir)) != NULL) {
		char *filename = ent->d_name;
		struct stat st;

		if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
			continue;

		if (stat(filename, &st) < 0) {
			closedir(dir);
			perror("Failed to get statistics while listing directory...\r\n");
			return NULL;
		}

		if (st.st_mode & S_IRUSR)
			DArray_push(fileList, filename);
	}

	return fileList;
}

char* getCurrentWorkingDirectory() {
	char *curDir = malloc(sizeof(char) * PATH_MAX);

	if (getcwd(curDir, sizeof(char) * PATH_MAX) == NULL)
		printf("Error in getting current directory...");

	return curDir;
}

int changeDirectory(char *path) {
	return chdir(path);
}

int getDirectoryListing(char directoryListBuf[], int directoryListBufLen) {
	DIR *dir;
	struct dirent *ent;
	int off = 0;

	if ((dir = opendir(".")) < 0) {
		perror("Failed to open current directory while listing...\r\n");
		return -1;
	}

	directoryListBuf[0] = '\0';

	while ((ent = readdir(dir)) != NULL) {
		char *filename = ent->d_name;
		struct stat st;
		char mode[] = "----------";
		struct passwd *pwd;
		struct group *grp;
		struct tm *ptm;
		char timebuf[BUFSIZ];
		int timelen;

		if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
			continue;

		if (stat(filename, &st) < 0) {
			closedir(dir);
			perror("Failed to get statistics while listing directory...\r\n");
			return -1;
		}

		if (S_ISDIR(st.st_mode))
			mode[0] = 'd';
		if (st.st_mode & S_IRUSR)
			mode[1] = 'r';
		if (st.st_mode & S_IWUSR)
			mode[2] = 'w';
		if (st.st_mode & S_IXUSR)
			mode[3] = 'x';
		if (st.st_mode & S_IRGRP)
			mode[4] = 'r';
		if (st.st_mode & S_IWGRP)
			mode[5] = 'w';
		if (st.st_mode & S_IXGRP)
			mode[6] = 'x';
		if (st.st_mode & S_IROTH)
			mode[7] = 'r';
		if (st.st_mode & S_IWOTH)
			mode[8] = 'w';
		if (st.st_mode & S_IXOTH)
			mode[9] = 'x';
		mode[10] = '\0';
		off += snprintf(directoryListBuf + off, directoryListBufLen - off,
				"%s ", mode);

		/* hard link number, this field is nonsense for ftp */
		off += snprintf(directoryListBuf + off, directoryListBufLen - off,
				"%d ", 1);

		/* user */
		if ((pwd = getpwuid(st.st_uid)) == NULL) {
			closedir(dir);
			return -1;
		}
		off += snprintf(directoryListBuf + off, directoryListBufLen - off,
				"%s ", pwd->pw_name);

		/* group */
		if ((grp = getgrgid(st.st_gid)) == NULL) {
			closedir(dir);
			return -1;
		}
		off += snprintf(directoryListBuf + off, directoryListBufLen - off,
				"%s ", grp->gr_name);

		/* size */
		off += snprintf(directoryListBuf + off, directoryListBufLen - off,
				"%*d ", 10, st.st_size);

		/* mtime */
		ptm = localtime(&st.st_mtime);
		if (ptm
				&& (timelen = strftime(timebuf, sizeof(timebuf), "%b %d %H:%S",
						ptm)) > 0) {
			timebuf[timelen] = '\0';
			off += snprintf(directoryListBuf + off, directoryListBufLen - off,
					"%s ", timebuf);
		} else {
			closedir(dir);
			return -1;
		}

		off += snprintf(directoryListBuf + off, directoryListBufLen - off,
				"%s\r\n", filename);

	}

	return off;
}
