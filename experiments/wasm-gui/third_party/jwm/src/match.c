/**
 * @file match.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Expression matching.
 *
 */

#include "jwm.h"
#include "match.h"

#include <regex.h>

/** Determine if expression matches pattern. */
char Match(const char *pattern, const char *expression)
{

   regex_t re;
   regmatch_t rm;
   int rc;

   if(!pattern && !expression) {
      return 1;
   } else if(!pattern || !expression) {
      return 0;
   }

   if(regcomp(&re, pattern, REG_EXTENDED) != 0) {
      return 0;
   }

   rc = regexec(&re, expression, 0, &rm, 0);

   regfree(&re);

   return rc == 0 ? 1 : 0;

}

