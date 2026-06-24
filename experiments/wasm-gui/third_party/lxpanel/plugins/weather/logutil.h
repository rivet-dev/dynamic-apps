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

#ifndef LXWEATHER_LOGUTIL_HEADER
#define LXWEATHER_LOGUTIL_HEADER

/**
 * Log Levels
 */
typedef enum
{
  LXW_NONE = 0,
  LXW_ERROR,
  LXW_DEBUG,
  LXW_VERBOSE,
  LXW_ALL
} LXWEATHER_LOGLEVEL;

/**
 * Initializes the logging subsystem
 *
 * @param pczPath Path to a file to log to (can be NULL for std{out|err},
 *                or 'syslog' for syslog)
 */
void initializeLogUtil(const char * pczPath);

/**
 * Cleans up the logging subsystem
 *
 */
void cleanupLogUtil();

/**
 * Logs the message using the specified level.
 *
 * @param level The level to log at
 * @param pczMsg Message to log
 */
void logUtil(LXWEATHER_LOGLEVEL level, const char * pczMsg, ...);

/**
 * Sets the maximum allowed log level
 *
 * @param level The level to use for all messages to follow.
 *
 * @return Previous value of the maximum log level.
 */
LXWEATHER_LOGLEVEL setMaxLogLevel(LXWEATHER_LOGLEVEL level);

#ifdef DEBUG
#define LXW_LOG(level, fmt, args...) logUtil(level, fmt, ##args)
#else
#define LXW_LOG(level, fmt, args...)
#endif

#endif
