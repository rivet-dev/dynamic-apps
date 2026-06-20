/**
 * @file error.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Error handling functions.
 *
 */

#include "jwm.h"
#include "error.h"
#include "main.h"

/** Log a fatal error and exit. */
void FatalError(const char *str, ...) {

   va_list ap;
   va_start(ap, str);

   Assert(str);

   fprintf(stderr, _("JWM: error: "));
   vfprintf(stderr, str, ap);
   fprintf(stderr, "\n");

   va_end(ap);

   exit(1);

}

/** Log a warning. */
void Warning(const char *str, ...) {

   va_list ap;
   va_start(ap, str);

   Assert(str);

   WarningVA(NULL, str, ap);

   va_end(ap);

}

/** Log a warning. */
void WarningVA(const char *part, const char *str, va_list ap) {

   Assert(str);

   fprintf(stderr, _("JWM: warning: "));
   if(part) {
      fprintf(stderr, "%s: ", part);
   }
   vfprintf(stderr, str, ap);
   fprintf(stderr, "\n");

}

/** Callback to handle errors from Xlib.
 * Note that if debug output is directed to an X terminal, emitting too
 * much output can cause a dead lock (this happens on HP-UX). Therefore
 * ShowCheckpoint isn't used by default.
 */
int ErrorHandler(Display *d, XErrorEvent *e) {

#ifdef DEBUG

   char buffer[64];
   char code[32];

#endif

   if(initializing) {
      if(e->request_code == X_ChangeWindowAttributes
         && e->error_code == BadAccess) {
         FatalError(_("display is already managed"));
      }
   }

#ifdef DEBUG

   if(!e) {
      fprintf(stderr, "XError: [no information]\n");
      return 0;
   }

   XGetErrorText(display, e->error_code, buffer, sizeof(buffer));
   Debug("XError: %s", buffer);

   snprintf(code, sizeof(code), "%d", e->request_code);
   XGetErrorDatabaseText(display, "XRequest", code, "?",
                         buffer, sizeof(buffer));
   Debug("   Request Code: %d (%s)", e->request_code, buffer);
   Debug("   Minor Code: %d", e->minor_code);
   Debug("   Resource ID: 0x%lx", (unsigned long)e->resourceid);
   Debug("   Error Serial: %lu", (unsigned long)e->serial);

#if 1
   ShowCheckpoint();
#endif

#endif

   return 0;

}
