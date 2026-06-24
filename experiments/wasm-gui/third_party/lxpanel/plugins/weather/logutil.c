/**
 * Copyright (c) 2012-2014 Piotr Sipika; see the AUTHORS file for more.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * See the COPYRIGHT file for more information.
 */

/* Provides logging utilities */

#include "logutil.h"

#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

static int g_Initialized = 0;
static LXWEATHER_LOGLEVEL g_Level = LXW_NONE;
static int g_FD = -1; /* -1 is syslog, 0 is std{out|err} */

/**
 * Initializes the logging subsystem
 *
 * @param pczPath Path to a file to log to (can be NULL for std{out|err},
 *                or 'syslog' for syslog)
 */
void 
initializeLogUtil(const char * pczPath)
{
#ifndef DEBUG
  return;
#endif

  if (g_Initialized)
    {
      return;
    }

  if (pczPath)
    {
      if (strncmp(pczPath, "syslog", 6) == 0)
        {
          /* syslog */
          openlog("LXWeather", LOG_NDELAY | LOG_PID, LOG_USER);
        }
      else if (strncmp(pczPath, "std", 3) == 0)
        {
          /* std{out|err} */
          g_FD = 0;
        }
      else
        {
          /* Attempt to open this file for writing */
          g_FD = open(pczPath, 
                      O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC, 
                      S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

          if (g_FD < 0)
            {
              /* Failed */
              fprintf(stderr, "LXWeather::initalizeLogUtil(): Failed to open %s: %s\n",
                      pczPath, strerror(errno));

              /* Initialized flag is 0, so no logging will happen */
              return;
            }

        }

    }
  else
    {
      /* stdout/err */
      g_FD = 0;
    }

  g_Initialized = 1;
}

/**
 * Cleans up the logging subsystem
 *
 */
void 
cleanupLogUtil()
{
#ifndef DEBUG
  return;
#endif

  if (g_Initialized)
    {
      switch (g_FD)
        {
        case -1:
          closelog();
          break;

        case 0:
          /* std{out|err} */
          break;

        default:
          /* Close the file */
          close(g_FD); /* Don't care about errors */
        }

      g_Initialized = 0;
    }

}

/**
 * Logs the message using the specified level.
 *
 * @param level The level to log at
 * @param pczMsg Message to log
 */
void 
logUtil(LXWEATHER_LOGLEVEL level, const char * pczMsg, ...)
{
#ifndef DEBUG
  return;
#endif

  if (g_Initialized && (level <= g_Level) && (g_Level > LXW_NONE))
    {
      va_list ap;
            
      va_start(ap, pczMsg);

      if (g_FD == -1)
        {
          int iSysLevel = (level == LXW_ERROR) ? LOG_ERR : LOG_NOTICE;
          
          vsyslog(iSysLevel, pczMsg, ap);
        }
      else
        {
          char cBuf[1024];
          
          pid_t myPid = getpid();

          /* This is not portable, due to pid_t... */
          size_t szBuf = snprintf(cBuf, sizeof(cBuf), "LXWeather [%ld] [%5s] ", 
                                  (long)myPid, 
                                  (level == LXW_ERROR) ? "ERROR" : "DEBUG");
          
          szBuf += vsnprintf(cBuf + szBuf, sizeof(cBuf) - szBuf, pczMsg, ap);
          
          szBuf += snprintf(cBuf + szBuf, sizeof(cBuf) - szBuf, "\n");

          if (g_FD == 0)
            {
              /* std{out|err} */

              if (level == LXW_ERROR)
                {
                  fprintf(stderr, "%s", cBuf);
                }
              else
                {
                  fprintf(stdout, "%s", cBuf);
                }
            }
          else
            {
              /* write to file */
              size_t wsz = write(g_FD, cBuf, szBuf);
              (void) wsz; /* to prevent compile warning */
            }
        }
           
      va_end(ap);

    }

}

/**
 * Sets the maximum allowed log level
 *
 * @param level The level to use for all messages to follow.
 *
 * @return Previous value of the maximum log level.
 */
LXWEATHER_LOGLEVEL 
setMaxLogLevel(LXWEATHER_LOGLEVEL level)
{
#ifndef DEBUG
  return g_Level;
#endif

  LXWEATHER_LOGLEVEL previous = g_Level;

  if (g_Initialized && level <= LXW_ALL)
    {
      g_Level = level;
    }

  return previous;
}
