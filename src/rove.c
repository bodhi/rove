/**
 * This file is part of rove.
 * rove is copyright 2007-2009 william light <visinin@gmail.com>
 *
 * rove is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rove is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rove.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config_parser.h"

#include "rove.h"
#include "file.h"
#include "jack.h"
#include "list.h"
#include "rmonome.h"
#include "util.h"
#include "session.h"
#include "settings.h"


#define DEFAULT_CONF_FILE_NAME  ".rove.conf"

#define DEFAULT_MONOME_COLUMNS  8
#define DEFAULT_MONOME_ROWS     8

#define DEFAULT_OSC_PREFIX      "rove"
#define DEFAULT_OSC_HOST_PORT   "8080"
#define DEFAULT_OSC_LISTEN_PORT "8000"


state_t state;

int is_numstr(char *str) {
	/* scan through the string looking for either a NULL (end of string)
	   or a non-numeric character */

	/* 48 is ASCII '0', 57 is ASCII '9'. tests one character. */
#define is_numeric(c) ((48 <= c) && (c <= 57))

	for(; *str && is_numeric(*str); str++);

	/* did we make it all the way through? */
	if( *str )
		return 0;

	return 1;
}

void usage() {
	printf("Usage: rove [OPTION]... session_file.rv\n"
		   "  -c, --monome-columns=COLUMNS\n"
		   "  -r, --monome-rows=ROWS\n"
		   "\n"
		   "  -p, --osc-prefix=PREFIX\n"
		   "  -h, --osc-host-port=PORT\n"
		   "  -l, --osc-listen-port=PORT\n\n");
}

static void monome_display_loop() {
	static int pblnk = 0;

	int j, group_count, next_bit;
	uint16_t dfield;

	struct timespec req;
	pattern_t *p;

	r_monome_t *monome = state.monome;
	group_t *g;
	file_t *f;

	req.tv_sec  = 0;
	req.tv_nsec = 1000000000 / 80; /* 80 fps */

	for(;;) {
		group_count = state.group_count;

		if( (p = state.pattern_rec) && p->step_delay )
			monome_led_set(
				p->monome->dev, p->monome->cols - 4 + p->idx, 0,
				((pblnk = (pblnk + 1) % p->step_delay) < ((p->step_delay / 2) + 1)));

		for( j = 0; j < group_count; j++ ) {
			g = &state.groups[j];
			f = g->active_loop;

			if( !f )
				continue;

			if( f->monome_out_cb )
				f->monome_out_cb(f, state.monome);
		}

		dfield = monome->dirty_field >> 1;

		for( j = 0; dfield; dfield >>= next_bit ) {
			next_bit = ffs(dfield);
			j += next_bit;

			f = (file_t *) state.monome->callbacks[j].data;

			if( f && f->monome_out_cb )
				f->monome_out_cb(f, state.monome);
			else
				monome->dirty_field &= ~(1 << j);
		}

		nanosleep(&req, NULL);
	}
}

static char *user_config_path() {
	char *home, *path;

	if( !(home = getenv("HOME")) )
		return NULL;

	asprintf(&path, "%s/%s", home, DEFAULT_CONF_FILE_NAME);
	return path;
}

static void exit_on_signal(int s) {
	exit(0);
}

static void cleanup() {
	r_monome_stop_thread(state.monome);
	r_monome_free(state.monome);

	r_jack_deactivate();
}

int main(int argc, char **argv) {
	char *session_file, c;
	int i;

	struct option arguments[] = {
		{"help",			no_argument,       0, 'u'}, /* for "usage", get it?  hah, hah... */
		{"monome-columns",	required_argument, 0, 'c'},
		{"monome-rows",		required_argument, 0, 'r'},
		{"osc-prefix", 		required_argument, 0, 'p'},
		{"osc-host-port", 	required_argument, 0, 'h'},
		{"osc-listen-port",	required_argument, 0, 'l'},
		{0, 0, 0, 0}
	};

	memset(&state, 0, sizeof(state_t));

	session_file = NULL;
	opterr = 0;

	while( (c = getopt_long(argc, argv, "uc:r:p:h:l:", arguments, &i)) > 0 ) {
		switch( c ) {
		case 'u':
			usage();
			exit(EXIT_FAILURE); /* should we exit after displaying usage? (i think so) */

		case 'c':
			state.config.cols = ((atoi(optarg) - 1) & 0xF) + 1;
			break;

		case 'r':
			state.config.rows = ((atoi(optarg) - 1) & 0xF) + 1;
			break;

		case 'p':
			if( *optarg == '/' ) /* remove the leading slash if there is one */
				optarg++;

			state.config.osc_prefix = strdup(optarg);
			break;

		case 'h':
			if( !is_numstr(optarg) )
				usage_printf_exit("error: \"%s\" is not a valid host port.\n\n", optarg);

			state.config.osc_host_port = strdup(optarg);
			break;

		case 'l':
			if( !is_numstr(optarg) )
				usage_printf_exit("error: \"%s\" is not a valid listen port.\n\n", optarg);

			state.config.osc_listen_port = strdup(optarg);
			break;
		}
	}

	if( settings_load(user_config_path()) )
		exit(EXIT_FAILURE);

        if (state.config.cols == 0) {
          printf("what's your monome config?\n");
          exit(EXIT_FAILURE);
        }

	state.group_count = state.config.cols - 4;
	state.patterns = list_new();
	list_init(&state.sessions);

	printf("\nhey, welcome to rove!\n\n"
		   "loading yr sessions:\n");

	while( optind < argc ) {
		session_file = argv[optind++];

		if( session_load(session_file) )
			printf("    error loading session file %s\n", session_file);
		else
			printf("    loaded %s\n", session_file);
	}

	if( stlist_is_empty(state.sessions) ) {
		fprintf(stderr, "\nYOU HAVE NO SESSIONS\nPLEASE TRY AGAIN\n\n");
		exit(EXIT_FAILURE);
	}

	if( r_jack_init() ) {
		fprintf(stderr, "error initializing JACK :(\n");
		exit(EXIT_FAILURE);
	}

#define ASSIGN_IF_UNSET(k, v) do { \
	if( !k ) \
		k = v; \
} while( 0 );

	ASSIGN_IF_UNSET(state.config.osc_prefix, DEFAULT_OSC_PREFIX);
	ASSIGN_IF_UNSET(state.config.osc_host_port, DEFAULT_OSC_HOST_PORT);
	ASSIGN_IF_UNSET(state.config.osc_listen_port, DEFAULT_OSC_LISTEN_PORT);
	ASSIGN_IF_UNSET(state.config.cols, DEFAULT_MONOME_COLUMNS);
	ASSIGN_IF_UNSET(state.config.rows, DEFAULT_MONOME_ROWS);

#undef ASSIGN_IF_UNSET

	session_activate(SESSION_T(state.sessions.head.next));

	if( r_monome_init() )
		exit(EXIT_FAILURE);

	if( r_jack_activate() )
		exit(EXIT_FAILURE);

	signal(SIGINT, exit_on_signal);
	atexit(cleanup);

	r_monome_run_thread(state.monome);
	monome_display_loop();

	return 0;
}
