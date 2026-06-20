/**
 * @file misc.h
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Miscellaneous functions and macros.
 *
 */

#ifndef MISC_H
#define MISC_H

/** Mapping between a string and integer.
 * This is used with FindValue and FindKey.
 * Note that mappings must be sorted.
 */
typedef struct {
    const char *key;
    int value;
} StringMappingType;

/** Get the length of an array. */
#define ARRAY_LENGTH( a ) (sizeof(a) / sizeof(a[0]))

/** Return the minimum of two values. */
#define Min( x, y ) ( (x) > (y) ? (y) : (x) )

/** Return the maximum of two values. */
#define Max( x, y ) ( (x) > (y) ? (x) : (y) )

/** Determine if a character is a space character.
 * @param ch The character to check.
 * @param lineNumber The line number to update.
 */
char IsSpace(char ch, unsigned int *lineNumber);

/** Perform shell-like macro path expansion.
 * @param path The path to expand (possibly reallocated).
 */
void ExpandPath(char **path);

/** Trim leading and trailing whitespace from a string.
 * @param str The string to trim.
 */
void Trim(char *str);

/** Copy a string.
 * Note that NULL is accepted. When provided NULL, NULL will be returned.
 * @param str The string to copy.
 * @return A copy of the string.
 */
char *CopyString(const char *str);

/** Read a float in a locale-independent way.
 * @param str The string containing the float.
 * @return The float.
 */
float ParseFloat(const char *str);

/** Find a value in a string mapping.
 * This uses binary search.
 * @param mapping The mapping.
 * @param key The item to find.
 * @param count The number of items in the mapping.
 * @return The value or -1 if not found.
 */
int FindValue(const StringMappingType *mapping, int count, const char *key);

/** Find a key in a string mapping.
 * This uses linear search.
 * @param mapping The mapping.
 * @param value The value to find.
 * @param count The number of items in the mapping.
 * @return The key or NULL if not found.
 */
const char *FindKey(const StringMappingType *mapping, int count, int value);

/** Case insensitive string compare. */
int StrCmpNoCase(const char *a, const char *b);

#endif /* MISC_H */
