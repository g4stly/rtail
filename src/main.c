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
#define TRNCATE (4)

#define DEF_BUF_SZ	(3)
#define DEF_DEL_BUF	(0)
#define DEF_SUP_ERR	(0)
#define DEF_TRNCATE	(0)

#define READ 	(0)
#define WRITE 	(1)


void parse_options(int argc, char **argv, int *args);
void child_main(int argc, char **argv, int *args, int *channel);
int rtail_main(int *args, int *channel);

int main(int argc, char ** argv)
{
	int pid, err;
	int channel[2];
	int args[] = { 1,
		DEF_BUF_SZ,
		DEF_DEL_BUF,
       		DEF_SUP_ERR,
		DEF_TRNCATE
	};

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
			if (!args[BUF_SZ]) args[BUF_SZ] = DEF_BUF_SZ;
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
		case 't':
			args[TRNCATE] = 1;
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
	setupterm(NULL, 1, NULL);

	char c;
	int line_len = 0;	// length of current line
	int line_cap = 8;	// capacity of current line
	int line_idx = 0;	// index
	int cols = COLS;
	char *line_buf[args[BUF_SZ]];
	FILE *input = fdopen(channel[READ], "r");
	if (!input) die("fdopen():");
	close(channel[WRITE]);

	putp(cursor_invisible);

	// initialize line buffer
	for (int i = 0; i < args[BUF_SZ]; i++) {
		line_buf[i] = malloc(line_cap * sizeof(**line_buf));
		if (!line_buf[i]) die("malloc():");
		memset(line_buf[i], 0, line_cap);
	}

	//TODO: optimize
	while ((c = fgetc(input)) != EOF) {
		// rollover case
		if (line_idx >= args[BUF_SZ]) {
			free(line_buf[0]);
			putp(tiparm(parm_up_cursor, args[BUF_SZ]));
			for (int i = 1; i < args[BUF_SZ]; i++) {
				putp(tiparm(clr_eol));
				printf("%s\n", line_buf[i - 1] = line_buf[i]);
				putp(carriage_return);
			}
			line_idx = args[BUF_SZ] - 1;
			line_buf[line_idx] = malloc(line_cap * sizeof(**line_buf));
			if (!line_buf[line_idx]) die("malloc():");
			line_cap = 8;
			line_len = 0;
		}
		// grow line case
		if (line_len >= line_cap - 2) {
			line_buf[line_idx] = realloc(
				line_buf[line_idx], 
				line_cap * 2 * sizeof(**line_buf)
			);
			if (!line_buf[line_idx]) die("realloc():");
			memset(line_buf[line_idx] + line_cap , 0, line_cap);
			line_cap = line_cap << 1;
		}
		// truncate / extra lines case
		if (c != '\n') {
			if (line_len < cols - 1) {
				line_buf[line_idx][line_len++] = c;
				continue;
			}
			if (args[TRNCATE]) {
				while ((c = fgetc(input)) != '\n');
			}
		}
		putp(clr_eol);
		printf("%s\n", line_buf[line_idx++]);
		line_cap = 8;
		line_len = 0;
	}

	putp(cursor_normal);
	reset_shell_mode();

	fclose(input);

	for (int i = 0; i < args[BUF_SZ]; i++) {
		if (line_buf[i]) {
			if (args[DEL_BUF]) {
				putp(cursor_up);
				putp(clr_eol);
			}
			free(line_buf[i]);
		}
	}

	return 0;
}
