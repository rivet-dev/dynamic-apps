/* libintl stub implementation: pass-through gettext family (NLS disabled). */
#include "libintl.h"

char *gettext(const char *msgid) { return (char *) msgid; }
char *dgettext(const char *d, const char *msgid) { (void) d; return (char *) msgid; }
char *dcgettext(const char *d, const char *msgid, int c) { (void) d; (void) c; return (char *) msgid; }

char *ngettext(const char *s1, const char *s2, unsigned long int n) {
    return (char *) (n == 1 ? s1 : s2);
}
char *dngettext(const char *d, const char *s1, const char *s2, unsigned long int n) {
    (void) d;
    return (char *) (n == 1 ? s1 : s2);
}
char *dcngettext(const char *d, const char *s1, const char *s2, unsigned long int n, int c) {
    (void) d; (void) c;
    return (char *) (n == 1 ? s1 : s2);
}

char *textdomain(const char *d) { return (char *) (d ? d : "messages"); }
char *bindtextdomain(const char *d, const char *dir) { (void) d; return (char *) dir; }
char *bind_textdomain_codeset(const char *d, const char *cs) { (void) d; return (char *) cs; }
