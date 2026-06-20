/**
 * @file error.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Header for the error functions.
 *
 */

#ifndef ERROR_H
#define ERROR_H

/** Display and error message and terminate the program.
 * @param str The format of the message to display.
 */
void FatalError(const char *str, ...);

/** Display a warning message.
 * @param str The format of the message to display.
 */
void Warning(const char *str, ...);

/** Display a warning message.
 * @param part A section identifier for the message.
 * @param str The format string of the message to display.
 * @param ap The argument list.
 */
void WarningVA(const char *part, const char *str, va_list ap);

/** Handle an XError event.
 * @param d The display on which the event occurred.
 * @param e The error event.
 * @return 0
 */
int ErrorHandler(Display *d, XErrorEvent *e);

#endif

