/**
 * @file match.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Expression matching.
 *
 */

#ifndef MATCH_H
#define MATCH_H

/** Check if an expression matches a pattern.
 * @param pattern The pattern to match against.
 * @param expression The expression to check.
 * @return 1 if there is a match, 0 otherwise.
 */
char Match(const char *pattern, const char *expression);

#endif /* MATCH_H */

