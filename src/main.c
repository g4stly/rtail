#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <curses.h>
#include <term.h>

#include "util.h"

#define ARG_IDX	(0)
#define BUF_SZ	(1)
#define DEL_BUF	(2)
#define SUP_ERR	(3)

#define DEFAULT_BUF_SZ	(3)

#define READ 	(0)
#define WRITE 	(1)


void parse_options(int argc, char **argv, int *args);
void child_main(int argc, char **argv, int *args, int *channel);
int rtail_main(int *args, int *channel);

int main(int argc, char ** argv)
{
	int pid, err;
	int channel[2];
	int args[] = { 1, DEFAULT_BUF_SZ, 0, 0 };

	err = pipe(channel);
	if (err < 0) die("pipe():");

	parse_options(argc, argv, args);

	pid = fork();
	if (pid == 0) child_main(argc, argv, args, channel);
	if (pid < 0) die("fork():");
	return rtail_main(args, channel);
}

void parse_options(int argc, char **argv, int *args)
{
	char *arg;
	int target = -1;
	for (args[ARG_IDX] = 1; args[ARG_IDX] < argc; args[ARG_IDX]++) {
		arg = argv[args[ARG_IDX]];

		switch (target) {
		case BUF_SZ:
			args[BUF_SZ] = atoi(arg);
			if (!args[BUF_SZ]) args[BUF_SZ] = DEFAULT_BUF_SZ;
			target = -1;
			continue;
		}

		if (arg[0] != '-') break;

		switch (arg[1]) {
		case 'b':
		case 'n':
			target = BUF_SZ;
			break;
		case 'd':
			args[DEL_BUF] = 1;
			break;
		case 'e':
			args[SUP_ERR] = 1;
			break;
		default:
			fprintf(stderr, "bad option: %s\n", arg);
			break;
		}
	}
	if (args[ARG_IDX] >= argc) {
		die("usage: %s [options] command ...\n", argv[0]);
	}
}

void child_main(int argc, char **argv, int *args, int *channel)
{
	close(channel[READ]);
	dup2(channel[WRITE], STDOUT_FILENO);
	close(channel[WRITE]);

	if (args[SUP_ERR]) {
		int null = open("/dev/null", O_WRONLY);
		dup2(null, STDERR_FILENO);
		close(null);
	}

	char *loc = NULL;
	char *path = NULL;
	char *exe = argv[args[ARG_IDX]];
	char *full_path = getenv("PATH");
	int path_sz, exe_sz = strlen(exe);

	// try cwd first
	if (exe_sz > 3 && exe[0] == '.' && exe[1] == '/') {
		execv(exe, argv + args[ARG_IDX]);
		die("execv(): %s:", exe);
	}

	for (loc = strtok(full_path, ":"); loc; loc = strtok(NULL, ":")) {
		path_sz = strlen(loc) + exe_sz + 1;
		path = malloc(path_sz * sizeof(*path) + 1);
		if (!path) die("malloc():");
		memset(path, 0, path_sz);

		snprintf(path, path_sz + 1, "%s/%s", loc, exe);
		execv(path, argv + args[ARG_IDX]);

		free(path);
		if (errno != ENOENT) {
			die("execv(): %s:", exe);
		}
	}
	die("execv(): %s:", exe);
}

int rtail_main(int *args, int *channel)
{

	size_t n = 0;
	char *line = NULL;
	int err, count = 0;
	int buf_sz = args[BUF_SZ];
	char *line_buffer[buf_sz];
	FILE *input = fdopen(channel[READ], "r");
	if (!input) die("fdopen():");
	close(channel[WRITE]);

	for (int i = 0; i < buf_sz; i++) {
		line_buffer[i] = NULL;
	}

	setupterm(NULL, 1, NULL);
	putp(cursor_invisible);

	while ((err = getline(&line, &n, input)) > 0) {
		if (count >= buf_sz) {
			free(line_buffer[0]);
			putp(tiparm(parm_up_cursor, buf_sz));
			for (int i = 1; i < buf_sz; i++) {
				putp(tiparm(clr_eol));
				printf("%s\n", line_buffer[i - 1] = line_buffer[i]);
				putp(carriage_return);
			}
			count = buf_sz - 1;
		}
		int x;
		int y = strlen(line) > 80 ? 80 : strlen(line);
		for (x = 0; x < y; x++) {
			if (line[x] == '\n') {
				line[x] = '\0';
				break;
			}
			if (x == 79) { line[x] = '\0'; }
		}
		putp(clr_eol);
		printf("%s\n", line_buffer[count++] = line);
		line = NULL;
		n = 0;
	}

	putp(cursor_normal);
	reset_shell_mode();

	fclose(input);

	if (err < 0) free(line);
	for (int i = 0; i < buf_sz; i++) {
		if (line_buffer[i]) {
			if (args[DEL_BUF]) {
				putp(cursor_up);
				putp(clr_eol);
			}
			free(line_buffer[i]);
		}
	}

	return 0;
}
