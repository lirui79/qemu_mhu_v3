// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

static int load_module(const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);
	int ret;

	if (fd < 0)
		return -1;
	ret = syscall(SYS_finit_module, fd, "", 0);
	close(fd);
	return ret;
}

static int split_args(char *line, char **argv, int max_args)
{
	int argc = 0;
	char *token;

	token = strtok(line, " \t\r\n");
	while (token && argc < max_args - 1) {
		argv[argc++] = token;
		token = strtok(NULL, " \t\r\n");
	}
	argv[argc] = NULL;
	return argc;
}

static int run_command(char **argv)
{
	pid_t child;
	int status;

	child = fork();
	if (!child) {
		execv(argv[0], argv);
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		_exit(127);
	}
	if (child < 0) {
		fprintf(stderr, "fork: %s\n", strerror(errno));
		return -1;
	}
	if (waitpid(child, &status, 0) < 0) {
		fprintf(stderr, "waitpid: %s\n", strerror(errno));
		return -1;
	}
	printf("linux_a76: %s status=%d\n", argv[0], status);
	return status;
}

static void command_loop(void)
{
	char line[128];
	char *argv[8];
	int argc;

	puts("linux_a76: manual mode");
	puts("linux_a76: type /bin/launch_kernel to run the runtime test");
	puts("linux_a76: commands: help, exit");

	while (1) {
		printf("linux_a76# ");
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin)) {
			clearerr(stdin);
			continue;
		}

		argc = split_args(line, argv, ARRAY_SIZE(argv));
		if (!argc)
			continue;
		if (!strcmp(argv[0], "help")) {
			puts("/bin/launch_kernel  run runtime launch test");
			puts("exit                stop init in pause loop");
			continue;
		}
		if (!strcmp(argv[0], "exit"))
			break;
		if (!strcmp(argv[0], "launch_kernel"))
			argv[0] = "/bin/launch_kernel";

		run_command(argv);
	}
}

static void autorun_launch_kernel(void)
{
	char *argv[] = { "/bin/launch_kernel", NULL };

	if (access("/etc/r52_gpu_autorun", R_OK))
		return;

	puts("linux_a76: autorun /bin/launch_kernel");
	run_command(argv);
}

int main(void)
{
	int retry;

	mkdir("/dev", 0755);
	mkdir("/proc", 0555);
	mkdir("/sys", 0555);
	mount("devtmpfs", "/dev", "devtmpfs", 0, "");
	mount("proc", "/proc", "proc", 0, "");
	mount("sysfs", "/sys", "sysfs", 0, "");

	puts("linux_a76: loading r52_gpu.ko");
	if (load_module("/lib/modules/r52_gpu.ko") < 0)
		fprintf(stderr, "finit_module: %s\n", strerror(errno));

	for (retry = 0; retry < 100 && access("/dev/r52_gpu", R_OK); retry++)
		usleep(100000);

	autorun_launch_kernel();
	command_loop();

	for (;;)
		pause();
}
