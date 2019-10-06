#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <term.h>
#include "util.h"

#define DEFAULT_BUF_SZ (3)
#define BUF_SZ 1

void parse_options(int argc, char **argv, char **command_str, int *buf_sz)
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
	int buf_sz = 3;
	char *command_str = NULL;
	parse_options(argc, argv, &command_str, &buf_sz);

	/*
	 * there are sort of several problems with this,
	 * this program will crash hard if the user
	 * specified commands fail in some way. there
	 * should be some way to respond to the failure
	 * of the below commands.
	 */

	system("rm -f /tmp/rtail_fifo");
	system("mkfifo /tmp/rtail_fifo");
	system(command_str);

	int err;
	size_t n = 0;
	int count = 0;
	char *line = NULL;
	char *line_buffer[buf_sz];
	FILE *queue = fopen("/tmp/rtail_fifo", "r");

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
		putp(clr_eol);
		printf("%s", line_buffer[count++] = line);
		line = NULL;
		n = 0;
	}
	if (err < 0) free(line);
	putp(cursor_visible);
	reset_shell_mode();
	fclose(queue);

	for (int i = 0; i < buf_sz; i++) {
		if (line_buffer[i]) free(line_buffer[i]);
	}

	system("rm -f /tmp/tempfifo");
	free(command_str);

	return 0;
}
