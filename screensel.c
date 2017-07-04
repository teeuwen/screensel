/*
 *
 * screensel
 *
 * Copyright (C) 2017 Bastiaan Teeuwen <bastiaan@mkcl.nl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 */

#include <limits.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Load defaults if parsing config file fails */
#define ALWAYS_RECOVER 0

#define A_PRIMARY	0
#define A_SECONDARY	1
#define A_MIRROR	2
#define A_EXTEND	3
#define A_QUERY		4

struct config {
	char	*primary;
	int	action;
	int	actionorder[4];
	double	actiondelay;
	char	*actioncmd;
	int	autoexit;
	int	direction;
	char	*conncmd;
	char	*disconncmd;
};

static struct config config;
static short action = -1, verbose;

static void die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);

	exit(1);
}

static char *strtrm(char *str)
{
	char *d;

	while (isspace(*str))
		str++;

	for (d = str + strlen(str); d >= str && isspace(*--d); *d = '\0');

	return str;
}

static void query(void)
{
	Display *conn;
	XRRScreenResources *scrres;
	XRROutputInfo *outin;
	XRRModeInfo *mode;
	int i, j, primary = 0;

	if (!(conn = XOpenDisplay(NULL)))
		die("unable to open display\n");

	if (!(scrres = XRRGetScreenResources(conn, DefaultRootWindow(conn))))
		die("unable to get screen resources\n");

	for (i = 0; i < scrres->noutput; i++) {
		if (!(outin = XRRGetOutputInfo(conn, scrres,
				scrres->outputs[i])))
			die("unable to get output information\n");

		for (j = 0; j < scrres->nmode; j++) {
			if (scrres->modes[j].id != outin->modes[0])
				continue;

			mode = &scrres->modes[j];

			primary = 0;

			if (config.primary) {
				if (strcmp(outin->name, config.primary) == 0)
					primary = 1;
			} else {
				if (strcmp(outin->name, "LVDS1") == 0 ||
						strcmp(outin->name,
						"eDP1") == 0)
					primary = 1;
			}

			printf("%s %s\n - res: %dx%d\n", outin->name,
					primary ? "(primary)" : "",
					mode->width, mode->height);
		}

	}
}

static void config_defaults(void)
{
	config.primary = NULL;
	config.action = 0;
	config.actionorder[0] = 0;
	config.actionorder[1] = 1;
	config.actionorder[2] = 2;
	config.actionorder[3] = 3;
	config.actiondelay = 1.0;
	config.actioncmd = NULL;
	config.autoexit = 0;
	config.direction = 1;
	config.conncmd = NULL;
	config.disconncmd = NULL;
}

static int config_enter(const char *name, const char *value)
{
	int l;

	if (strcmp(name, "primary") == 0) {
		if (!(config.primary = realloc(config.primary, strlen(value))))
			return 2;

		if (*value == '"') {
			l = strlen(value) - 2;
			strncpy(config.primary, value + 1, l);
			config.primary[l] = '\0';
		} else {
			strcpy(config.primary, value);
		}
	} else if (strcmp(name, "action") == 0) {
		if (!isdigit(*value))
			return 2;

		config.action = strtol(value, NULL, 10);
	} else if (strcmp(name, "actionorder") == 0) {
		/* XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX TEMP */
		config.actionorder[0] = 0;
		config.actionorder[1] = 1;
		config.actionorder[2] = 2;
		config.actionorder[3] = 3;
		/* XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX TEMP */
	} else if (strcmp(name, "actiondelay") == 0) {
		if (!isdigit(*value))
			return 2;

		config.actiondelay = strtod(value, NULL);
	} else if (strcmp(name, "actioncmd") == 0) {
		if (!(config.actioncmd =
				realloc(config.actioncmd, strlen(value))))
			return 2;

		if (*value == '"') {
			l = strlen(value) - 2;
			strncpy(config.actioncmd, value + 1, l);
			config.actioncmd[l] = '\0';
		} else {
			strcpy(config.actioncmd, value);
		}
	} else if (strcmp(name, "autoexit") == 0) {
		if (!isdigit(*value))
			return 2;

		config.autoexit = strtol(value, NULL, 10);
	} else if (strcmp(name, "direction") == 0) {
		if (!isdigit(*value))
			return 2;

		if (config.direction == 1)
			config.direction = strtol(value, NULL, 10);
	} else if (strcmp(name, "conncmd") == 0) {
		if (!(config.conncmd = realloc(config.conncmd, strlen(value))))
			return 2;

		if (*value == '"') {
			l = strlen(value) - 2;
			strncpy(config.conncmd, value + 1, l);
			config.conncmd[l] = '\0';
		} else {
			strcpy(config.conncmd, value);
		}
	} else if (strcmp(name, "disconncmd") == 0) {
		if (!(config.disconncmd =
				realloc(config.disconncmd, strlen(value))))
			return 2;

		if (*value == '"') {
			l = strlen(value) - 2;
			strncpy(config.disconncmd, value + 1, l);
			config.disconncmd[l] = '\0';
		} else {
			strcpy(config.disconncmd, value);
		}
	} else {
		return 1;
	}

	return 0;
}

static void config_free(void)
{
	free(config.primary);
	free(config.actioncmd);
	free(config.conncmd);
	free(config.disconncmd);
}

static void config_parse(char *path)
{
	char *buf = NULL, *tbuf, *nbuf = NULL, *tnbuf;
	size_t l, n = 0;
	ssize_t i, s;
	FILE *fp;

	if (!*path) {
		if (strcpy(path, getenv("XDG_CONFIG_HOME")))
			strcat(path, "/screensel/config");
		else
			strcpy(path, "~/.config/screensel/config");
	}

	if (verbose)
		fprintf(stderr, "config path: %s\n", path);

	config_defaults();

	if (!(fp = fopen(path, "r"))) {
		if (verbose)
			fprintf(stderr, "no config file found, "
					"loading defaults\n");

		return;
	}

	if (!(nbuf = malloc(12))) {
		fprintf(stderr, "out of memory\n");

		goto err;
	}

	for (l = 1; (s = getline(&buf, &n, fp)) != -1; l++) {
		for (i = 0; buf[i] && isspace(buf[i]); i++);

		if (!*(buf + i) || *(buf + i) == '#')
			continue;

		if (verbose)
			fprintf(stderr, "%ld: ", l);

		for (; buf[i] && buf[i] != '='; i++);

		if (i++ >= s) {
			fprintf(stderr, "%ld: syntax error: missing '='\n", l);
			goto err;
		}

		if (i > 12 && !(nbuf = realloc(nbuf, i - 1))) {
			fprintf(stderr, "out of memory\n");
			goto err;
		}

		memset(nbuf, 0, i);
		strncpy(nbuf, buf, i - 1);
		tbuf = strtrm(buf + i);
		tnbuf = strtrm(nbuf);

		if (verbose)
			fprintf(stderr, "%s = %s\n", tnbuf, tbuf);

		if (i >= s) {
			fprintf(stderr, "%ld: syntax error: missing value\n",
					l);
			goto err;
		}

		switch (config_enter(tnbuf, tbuf)) {
		case 1:
			fprintf(stderr, "%ld: syntax error, invalid name "
					"%s\n", l, tnbuf);
			goto err;
		case 2:
			fprintf(stderr, "%ld: syntax error, invalid value "
					"%s for %s\n", l, tbuf, tnbuf);
			goto err;
		}
	}

	free(nbuf);
	free(buf);

	fclose(fp);

	return;

err:
	config_free();
	free(nbuf);
	free(buf);

	fclose(fp);

#if ALWAYS_RECOVER
	fprintf(stderr, "loading defaults\n");

	config_defaults();
#else
	exit(1);
#endif
}

static void version(char *filename)
{
	printf("%s: screensel (c) 2017 Bastiaan Teeuwen\n", filename);
}

static void usage(void)
{
	printf(
"Usage:\n"
" screensel [options]\n"
"\n"
"Options:\n"
" -c, --config=PATH             path to configuration file\n"
/* " -d, --daemon                  start in daemon mode\n" */
" -e, --extend=DIRECTION        extend primary display to: 0 (left),\n"
"                                 1 (right), 2 (top), 3 (bottom) or the\n"
"                                 default direction specified in the\n"
"                                 configuration file\n"
" -h, --help                    show this message\n"
" -m, --mirror                  mirror the primary to the secondary display\n"
" -p, --primary                 use primary display only\n"
" -q, --query                   query displays\n"
" -s, --secondary               use secondary display only\n"
" -v, --verbose                 verbose output\n"
" -V, --version                 show version information\n");
}

static const char *options_short = "cehmpqsvV";
static const struct option options_long[] = {
	{ "config",	1, NULL, 'c' },
	{ "extend",	1, NULL, 'e' },
	{ "help",	0, NULL, 'h' },
	{ "mirror",	0, NULL, 'm' },
	{ "primary",	0, NULL, 'p' },
	{ "query",	0, NULL, 'q' },
	{ "secondary",	0, NULL, 's' },
	{ "verbose",	0, NULL, 'v' },
	{ "version",	0, NULL, 'V' },
	{ NULL,		0, NULL, 0 }
};

int main(int argc, char **argv)
{
	char path[PATH_MAX], c;
	int i;

	while ((c = getopt_long(argc, argv,
			options_short, options_long, &i)) != -1) {
		switch (c) {
		case 'c':
			strcpy(path, optarg);
			break;
		case 'e':
			action = 3;

			if (!isdigit(*optarg))
				die("invalid direction: %s\n", optarg);
			config.direction = strtol(optarg, NULL, 10);
			break;
		case 'h':
			usage();
			return 0;
		case 'm':
			action = 2;
			break;
		case 'p':
			action = 0;
			break;
		case 'q':
			action = 4;
			break;
		case 's':
			action = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			version(*argv);
			return 0;
		}
	}

	if (action < 0) {
		usage();
		return 1;
	}

	config_parse(path);

	switch (action) {
	case A_QUERY:
		query();
		break;
	}

	return 0;
}
