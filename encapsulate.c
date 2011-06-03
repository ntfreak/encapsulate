// required for fscanf "a" operator. and this tool is linux only anyway
#define _GNU_SOURCE

#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mount.h>

#ifndef MS_REC
#define MS_REC 16384
#endif

static int
cmpstring(const void *p1, const void *p2)
{
	return strcmp(*(char **) p1, *(char **) p2);
}

int mount_idemp(const char *path, const char *into)
{
	char *real = realpath(path, NULL);
	char *unreal = malloc(strlen(into)+strlen(real)+2);
	sprintf(unreal, "%s/%s", into, real);

	if (0 != mount(real, unreal, NULL, MS_BIND, NULL)) {
		perror("mounting writable tree failed");
		return 1;
	}

	return 0;
}

// usage: encapsulate chroot-dir writable-dir command-line ...
int main(int argc, char **argv)
{
	FILE *mounttable;

	if (argc < 3) {
		fprintf(stderr, "not enough arguments\n");
		return 1;
	}
	char **newargv = malloc(sizeof(char*)*(argc-3));
	memset(newargv, 0, sizeof(char*)*(argc-3));
	memcpy(newargv, argv+3, sizeof(char*)*(argc-3));

	char *chroot_path = realpath(argv[1], NULL);

	int uid = getuid();
	int euid = geteuid();

	setuid(0);

	unshare(CLONE_NEWIPC);
	unshare(CLONE_NEWNET);
	unshare(CLONE_NEWNS);
	unshare(CLONE_NEWPID); 

	if (0 != mount("/", chroot_path, NULL, MS_BIND | MS_REC, NULL)) {
		perror("mount failed");
		return 1;
	}
	if (0 != mount("/", chroot_path, NULL, MS_REMOUNT | MS_RDONLY | MS_BIND | MS_REC, NULL)) {
		perror("remount failed");
		return 1;
	}

	char *writables = argv[2];
	char *endp = writables;
	while (endp) {
		char *mountp = writables;
		endp = strchr(writables, '|');
		if (endp != NULL) {
			*endp = '\0';
			writables = endp+1;
		}
		
		if (0 != mount_idemp(mountp, chroot_path)) {
			return 1;
		}
	}

	chroot(chroot_path);
	chdir(get_current_dir_name());
	setuid(uid);
	seteuid(euid);
	execvp(newargv[0], newargv);
	return 0;
}
