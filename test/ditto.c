/****************************************************************************
 * Copyright (c) 1998-2007,2008 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/*
 * Author: Thomas E. Dickey <dickey@clark.net> 1998
 *
 * $Id: ditto.c,v 1.19 2008/04/12 23:37:32 tom Exp $
 *
 * The program illustrates how to set up multiple screens from a single
 * program.  Invoke the program by specifying another terminal on the same
 * machine by specifying its device, e.g.,
 *	ditto /dev/ttyp1
 */
#include <test.priv.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef USE_XTERM_PTY
#include USE_OPENPTY_HEADER
#endif

typedef struct {
    FILE *input;
    FILE *output;
    SCREEN *screen;
} DITTO;

static void
failed(const char *s)
{
    perror(s);
    ExitProgram(EXIT_FAILURE);
}

static void
usage(void)
{
    fprintf(stderr, "usage: ditto [terminal1 ...]\n");
    ExitProgram(EXIT_FAILURE);
}

static FILE *
open_tty(char *path)
{
    FILE *fp;
#ifdef USE_XTERM_PTY
    int amaster;
    int aslave;
    char slave_name[1024];
    char s_option[1024];
    char *leaf;

    if (openpty(&amaster, &aslave, slave_name, 0, 0) != 0)
	failed("openpty");
    if ((leaf = strrchr(slave_name, '/')) == 0) {
	errno = EISDIR;
	failed(slave_name);
    }
    sprintf(s_option, "-S%s/%d", slave_name, aslave);
    if (fork()) {
	execlp("xterm", "xterm", s_option, "-title", path, (char *) 0);
	_exit(0);
    }
    fp = fdopen(amaster, "r+");
#else
    struct stat sb;

    if (stat(path, &sb) < 0)
	failed(path);
    if ((sb.st_mode & S_IFMT) != S_IFCHR) {
	errno = ENOTTY;
	failed(path);
    }
    fp = fopen(path, "r+");
    if (fp == 0)
	failed(path);
    printf("opened %s\n", path);
#endif
    return fp;
}

static int
close_screen(SCREEN *sp GCC_UNUSED, void *arg GCC_UNUSED)
{
    (void) sp;
    (void) arg;
    return endwin();
}

static int
read_screen(SCREEN *sp GCC_UNUSED, void *arg GCC_UNUSED)
{
    return getch();
}

static int
write_screen(SCREEN *sp GCC_UNUSED, void *arg GCC_UNUSED)
{
    addstr((char *) arg);
    refresh();
    return OK;
}

static void
show_ditto(DITTO * data, int count, char *msg)
{
    int n;

    for (n = 0; n < count; n++) {
	USING_SCREEN(data[n].screen, write_screen, (void *) msg);
    }
}

int
main(int argc GCC_UNUSED,
     char *argv[]GCC_UNUSED)
{
    int j;
    int count;
    DITTO *data;

    if (argc <= 1)
	usage();

    if ((data = typeCalloc(DITTO, argc)) == 0)
	failed("calloc data");

    data[0].input = stdin;
    data[0].output = stdout;
    for (j = 1; j < argc; j++) {
	data[j].input =
	    data[j].output = open_tty(argv[j]);
    }

    /*
     * If we got this far, we have open connection(s) to the terminal(s).
     * Set up the screens.
     */
    for (j = 0; j < argc; j++) {
	data[j].screen = newterm((char *) 0,	/* assume $TERM is the same */
				 data[j].output,
				 data[j].input);
	if (data[j].screen == 0)
	    failed("newterm");
	cbreak();
	noecho();
	scrollok(stdscr, TRUE);
	nodelay(stdscr, TRUE);
    }

    /*
     * Loop, reading characters from any of the inputs and writing to all
     * of the screens.
     */
    for (count = 0;; ++count) {
	char message[80];
	int ch;
	int which = (count % argc);

	napms(20);
	ch = USING_SCREEN(data[which].screen, read_screen, 0);
	if (ch == ERR) {
	    continue;
	}
	if (ch == CTRL('D'))
	    break;
	sprintf(message, "from[%d:%d] '%c' (%#x)\n", count, which, ch, ch);
	show_ditto(data, argc, message);
    }

    /*
     * Cleanup and exit
     */
    for (j = argc - 1; j >= 0; j--) {
	USING_SCREEN(data[j].screen, close_screen, 0);
	fprintf(data[j].output, "**Closed\r\n");
	fflush(data[j].output);
	fclose(data[j].output);
	delscreen(data[j].screen);
    }
    ExitProgram(EXIT_SUCCESS);
}
