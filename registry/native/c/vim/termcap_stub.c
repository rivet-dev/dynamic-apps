/* termcap_stub.c — minimal termcap API so vim links and falls back to its
 * builtin terminal entries. tgetent() returns 0 ("no entry") which makes vim
 * use its compiled-in builtin termcap for $TERM. tputs writes capability
 * strings straight to the output fn (vim passes its own putc). */
#include <stddef.h>
#include <unistd.h>

char PC = '\0';
char *UP = (char *)0;
char *BC = (char *)0;
short ospeed = 0;

int tgetent(char *bp, const char *name) {
	(void)bp;
	(void)name;
	return 0; /* no external entry -> vim uses builtin termcap */
}

int tgetflag(const char *id) {
	(void)id;
	return 0;
}

int tgetnum(const char *id) {
	(void)id;
	return -1;
}

char *tgetstr(const char *id, char **area) {
	(void)id;
	(void)area;
	return (char *)0;
}

char *tgoto(const char *cap, int col, int row) {
	(void)cap;
	(void)col;
	(void)row;
	return (char *)0;
}

int tputs(const char *str, int affcnt, int (*putc_fn)(int)) {
	(void)affcnt;
	if (str && putc_fn) {
		for (const char *p = str; *p; p++) {
			putc_fn((int)(unsigned char)*p);
		}
	}
	return 0;
}
