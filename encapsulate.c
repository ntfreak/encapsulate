/*
 * Copyright (c) 2011, 2013 Patrick Georgi <patrick@georgi-clan.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/sched.h>

static int
cmpstring(const void *p1, const void *p2)
{
	return strcmp(*(char **) p1, *(char **) p2);
}

int is_dir(const char *path)
{
	struct stat fileinfo;
	int res = stat(path, &fileinfo);
	if (res != 0) return 0;
	if (!S_ISDIR(fileinfo.st_mode)) return 0;
	return 1;
}

int mount_idemp(const char *path, const char *into)
{
	char *real = realpath(path, NULL);
	if (real == NULL) {
		printf("%s is not a directory\n", path);
		return 1;
	}
	char *unreal = malloc(strlen(into)+strlen(real)+2);
	sprintf(unreal, "%s/%s", into, real);

	if (!is_dir(real)) {
		printf("%s is not a directory\n", real);
		return 1;
	}

	if (mount(real, unreal, NULL, MS_BIND, NULL) != 0) {
		perror("mounting writable tree failed");
		return 1;
	}

	return 0;
}

/* Remounts all the bind-mounted paths read-only.
   We mount writable versions on top later.
   This function is necessary since Linux doesn't
   support remount | bind | recursive. */
int remount_readonly(const char *chroot_path)
{
	FILE *file = fopen("/proc/mounts", "r");
	while (!feof(file)) {
		char *path;
		fscanf(file, "%*s %ms %*s %*s %*d %*d\n", &path);
		if (strstr(path, chroot_path) == path) {
			if (mount("", path, NULL, MS_REMOUNT | MS_RDONLY | MS_BIND, NULL) != 0) {
				return -1;
			}
		}
	}
	fclose(file);
	return 0;
}

// usage: encapsulate writable-dir command-line ...
int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "not enough arguments\n");
		return 1;
	}
	char **newargv = malloc(sizeof(char*)*(argc-1));
	memset(newargv, 0, sizeof(char*)*(argc-1));
	memcpy(newargv, argv+2, sizeof(char*)*(argc-2));

	char *chroot_path = mkdtemp(strdup("/tmp/encapsulate.XXXXXX"));

	if ((chroot_path == NULL) || !is_dir(chroot_path)) {
		printf("chroot directory is incorrect\n");
		return 1;
	}

	int uid = getuid();
	int euid = geteuid();

	setuid(0);

	int pid = fork();
	if (pid != 0) {
		int status;
		wait(&status);
		rmdir(chroot_path);
		return(WEXITSTATUS(status));
	}

	/* PID namespace only works on clone(), so do that later */
	if (unshare(CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_FILES | CLONE_NEWUTS | CLONE_SYSVSEM) != 0) {
		perror("creating private namespace failed.");
		return 1;
	}

	/* make sure recursive mounts terminate */
	if (mount("", "/", NULL, MS_PRIVATE | MS_REC, NULL) != 0) {
		perror("unsharing mountpoints failed");
		return 1;
	}

	if (mount("/", chroot_path, NULL, MS_BIND | MS_REC, NULL) != 0) {
		perror("mount failed");
		return 1;
	}

	if (remount_readonly(chroot_path) != 0) {
		perror("remount readonly failed");
		return 1;
	}

	char *writables = argv[1];
	char *endp = writables;
	while (endp) {
		char *mountp = writables;
		endp = strchr(writables, '|');
		if (endp != NULL) {
			*endp = '\0';
			writables = endp+1;
		}

		if (mount_idemp(mountp, chroot_path) != 0) {
			return 1;
		}
	}

	const char *current_path = getcwd(NULL, 0);
	chroot(chroot_path);
	chdir(current_path);
	setuid(uid);
	seteuid(euid);
	execvp(newargv[0], newargv);
	return 0;
}
