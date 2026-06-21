/* libintl stub for wasm32-wasip1: NLS is disabled in our GLib build, so gettext() and friends are
 * pass-throughs (return the original msgid). This satisfies GLib's `dependency('intl')` without
 * cross-compiling the full GNU gettext runtime. */
#ifndef LIBINTL_WASM_STUB_H
#define LIBINTL_WASM_STUB_H

#ifdef __cplusplus
extern "C" {
#endif

char *gettext(const char *msgid);
char *dgettext(const char *domainname, const char *msgid);
char *dcgettext(const char *domainname, const char *msgid, int category);
char *ngettext(const char *msgid1, const char *msgid2, unsigned long int n);
char *dngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n);
char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2,
                 unsigned long int n, int category);
char *textdomain(const char *domainname);
char *bindtextdomain(const char *domainname, const char *dirname);
char *bind_textdomain_codeset(const char *domainname, const char *codeset);

#ifdef __cplusplus
}
#endif

#endif /* LIBINTL_WASM_STUB_H */
