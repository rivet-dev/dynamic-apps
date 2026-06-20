/**
 * @file timing.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Timing functions.
 *
 */

#ifndef TIMING_H
#define TIMING_H

/** Initializer for TimeType to indicate that it is not set. */
#define ZERO_TIME { 0, 0 }

/** Structure to represent time since January 1, 1970 GMT. */
typedef struct TimeType {

   unsigned long seconds;  /**< Seconds. */
   int ms;                 /**< Milliseconds. */

} TimeType;

/** Get the current time.
 * @param t The TimeType to fill.
 */
void GetCurrentTime(TimeType *t);

/** Get the difference between two times.
 * Note that the times must be normalized.
 * @param t1 The first time.
 * @param t2 The second time.
 * @return The difference in milliseconds (maximum of 60000 ms).
 */
unsigned long GetTimeDifference(const TimeType *t1, const TimeType *t2);

/** Get a time string.
 * Note that the string returned is a static value and should not be
 * deleted. Therefore, this function is not thread safe.
 * @param format The format to use for the string.
 * @param zone The timezone in tzset() format to use (defaults to local)
 * @return The time string.
 */
const char *GetTimeString(const char *format, const char *zone);

#endif /* TIMING_H */

