#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <curses.h>
#include <term.h>

#include "util.h"

#define DEFAULT_BUF_SZ (3)
#define BUF_SZ 1

static const char *const FIFO_PATH = "/tmp/rtail_fifo";

void parse_options(int argc, char **argv, char **command_str, int *buf_sz, int *del_buf)
{
	int len, arg = 1, target = 0;
	for (arg = 1; arg < argc; arg++) {
		switch (target) {
		case BUF_SZ:
			*buf_sz = atoi(argv[arg]);
			if (!*buf_sz) *buf_sz = DEFAULT_BUF_SZ;
			target = 0;
			continue;
		}

		if (argv[arg][0] != '-') break;

		switch (argv[arg][1]) {
		case 'b':
			target = BUF_SZ;
			break;
		case 'd':
			*del_buf = 1;
			break;
		default:
			fprintf(stderr, "bad option: %s\n", argv[arg]);
			break;
		}
	}

	if (arg >= argc) {
		die("usage: %s [options] command ...\n", argv[0]);
	}


	len = strlen(argv[arg]);
	*command_str = malloc(len + 1);
	if (!*command_str) die ("malloc():");

	memset(*command_str, 0, len + 1);
	strncpy(*command_str, argv[arg], len);

	for (arg += 1; arg < argc; arg++) {
		len += strlen(argv[arg]) + 1;
		*command_str = realloc(*command_str, len + 1);
		if (!*command_str) die("realloc():");
		strcat(*command_str, " ");
		strcat(*command_str, argv[arg]);
	}

	// TODO: maybe avoid duplicate literals
	len += strlen(" > /tmp/rtail_fifo &");
	*command_str = realloc(*command_str, len + 1);
	if (!*command_str) die("realloc():");
	strcat(*command_str, " > /tmp/rtail_fifo &");

}

int main(int argc, char ** argv)
{
	char *command_str = NULL;
	int del_buf = 0, buf_sz = DEFAULT_BUF_SZ;
	parse_options(argc, argv, &command_str, &buf_sz, &del_buf);

	/*
	 * there are sort of several problems with this,
	 * there should be some way to respond to the failure
	 * of the below commands.
	 */

	int err;

	err = remove(FIFO_PATH);
	if (err < 0 && errno != ENOENT) die("remove():");

	err = mkfifo(FIFO_PATH, S_IRWXU);
	if (err < 0) die("mkfifo():");

	system(command_str);

	size_t n = 0;
	int count = 0;
	char *line = NULL;
	char *line_buffer[buf_sz];
	FILE *queue = fopen(FIFO_PATH, "r");
	if (!queue) die("fopen():");

	for (int i = 0; i < buf_sz; i++) {
		line_buffer[i] = NULL;
	}

	setupterm(NULL, 1, NULL);
	putp(cursor_invisible);
	while ((err = getline(&line, &n, queue)) > 0) {
		if (count >= buf_sz) {
			free(line_buffer[0]);
			putp(tiparm(parm_up_cursor, buf_sz));
			for (int i = 1; i < buf_sz; i++) {
				putp(tiparm(clr_eol));
				printf("%s", line_buffer[i - 1] = line_buffer[i]);
				putp(carriage_return);
			}
			count = buf_sz - 1;
		}
		int x;
		int y = strlen(line) > 80 ? 80 : strlen(line);
		for (x = 0; x < y; x++) {
			if (line[x] == '\n') {
				line[x+1] = '\0';
				break;
			}
		}
		if (x == 79) { line[x] = '\0'; }
		putp(clr_eol);
		printf("%s", line_buffer[count++] = line);
		line = NULL;
		n = 0;
	}

	fclose(queue);
	remove(FIFO_PATH);
	if (err < 0) free(line);

	for (int i = 0; i < buf_sz; i++) {
		if (line_buffer[i]) {
			if (del_buf) {
				putp(cursor_up);
				putp(clr_eol);
			}
			free(line_buffer[i]);
		}
	}

	putp(cursor_normal);
	reset_shell_mode();
	free(command_str);

	return 0;
}
