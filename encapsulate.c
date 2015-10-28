/*
 * Copyright (c) 2011, Patrick Georgi <patrick@georgi-clan.de>
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

// required for fscanf "a" operator. and this tool is linux only anyway
#define _GNU_SOURCE

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

enum {
	IOPRIO_WHO_PROCESS = 1,
	IOPRIO_WHO_PGRP,
	IOPRIO_WHO_USER
};

enum {
	IOPRIO_CLASS_NONE,
	IOPRIO_CLASS_RT,
	IOPRIO_CLASS_BE,
	IOPRIO_CLASS_IDLE
};

#define IOPRIO_CLASS_SHIFT      (13)
#define IOPRIO_PRIO_VALUE(class, data)  (((class) << IOPRIO_CLASS_SHIFT) | data)

#include <sys/syscall.h>
//#include <linux/ioprio.h>

#ifndef MS_REC
#define MS_REC 16384
#endif

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

int mount_hide(const char *path, const char *into)
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

	if (mount("/tmp", unreal, NULL, MS_BIND, NULL) != 0) {
		perror("mounting temp tree failed");
		return 1;
	}

	if (mount("/tmp", unreal, NULL, MS_REMOUNT | MS_RDONLY | MS_BIND, NULL) != 0) {
		perror("remounting temp tree failed");
		return 1;
	}

	return 0;
}

// usage: encapsulate chroot-dir writable-dir command-line ...
int main(int argc, char **argv)
{
	FILE *mounttable;

	if (argc < 2) {
		fprintf(stderr, "not enough arguments\n");
		return 1;
	}
	char **newargv = malloc(sizeof(char*)*(argc-2));
	memset(newargv, 0, sizeof(char*)*(argc-2));
	memcpy(newargv, argv+2, sizeof(char*)*(argc-2));

	char *chroot_path = mkdtemp(strdup("/tmp/encapsulate.XXXXXX"));

	if ((chroot_path == NULL) || !is_dir(chroot_path)) {
		printf("chroot directory is incorrect\n");
		return 1;
	}

	int uid = getuid();
	int euid = geteuid();

	if (setuid(0) != 0) {
		//perror("setuid failed");
	}

	int pid = fork();
	if (pid != 0) {
		int status;
		wait(&status);
		rmdir(chroot_path);
		return(WEXITSTATUS(status));
	}

	if (unshare(CLONE_NEWIPC) != 0) {
		//perror("CLONE_NEWIPC");
	}
	if (unshare(CLONE_NEWNET) != 0) {
		//perror("CLONE_NEWNET");
	}
	if (unshare(CLONE_NEWNS) != 0) {
		//perror("CLONE_NEWNS");
	}
	if (unshare(CLONE_NEWPID) != 0) {
		//perror("CLONE_NEWPID");
	}

	if (mount("/", chroot_path, NULL, MS_BIND | MS_REC, NULL) != 0) {
		perror("mount failed");
		return 1;
	}
	if (mount("/", chroot_path, NULL, MS_REMOUNT | MS_RDONLY | MS_BIND | MS_REC, NULL) != 0) {
		perror("remount failed");
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

	/* get home dir and make sure .ssh dir is not visible */
	char *home = getenv("HOME");
	if (home != NULL) {
		char *homedir = malloc(strlen(home)+strlen(".ssh")+2);
		sprintf(homedir, "%s/%s", home, ".ssh");
		if (mount_hide(homedir, chroot_path) != 0) {
			return 1;
		}
	}

	if (chroot(chroot_path) != 0) {
		//perror("chroot failed");
	}
	if (chdir(get_current_dir_name()) != 0) {
		//perror("chdir failed");
	}
	if (setuid(uid) != 0) {
		//perror("setuid failed");
	}
	if (seteuid(euid) != 0) {
		//perror("seteuid failed");
	}
	//nice(19);
	setpriority(PRIO_PROCESS, 0, 19);
	//ioprio_set(IOPRIO_WHO_PROCESS, 0, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0));
	syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, 0, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_IDLE, 0));
	//syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, 0, IOPRIO_PRIO_VALUE(IOPRIO_CLASS_BE, 7));
	execvp(newargv[0], newargv);
	return 0;
}
