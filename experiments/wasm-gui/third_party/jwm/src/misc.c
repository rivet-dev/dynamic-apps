/**
 * @file misc.c
 * @author Joe Wingbermuehle
 * @date 2004-2006
 *
 * @brief Miscellaneous functions and macros.
 *
 */

#include "jwm.h"
#include "misc.h"
#include "debug.h"

static char ToLower(char ch);
static char IsSymbolic(char ch);
static char *GetSymbolName(const char *str);
static void ReplaceSymbol(char **str, unsigned int offset,
                          const char *name, const char *value);

/** Determine if a character is a space character. */
char IsSpace(char ch, unsigned int *lineNumber)
{
   switch(ch) {
   case ' ':
   case '\t':
   case '\r':
      return 1;
   case '\n':
      *lineNumber += 1;
      return 1;
   default:
      return 0;
   }
}

/** Convert to lower case. */
char ToLower(char ch)
{
   /* This should work in either ASCII or EBCDIC. */
   if(   (ch >= 'A' && ch <= 'I')
      || (ch >= 'J' && ch <= 'R')
      || (ch >= 'S' && ch <= 'Z')) {
      return ch - 'A' + 'a';
   } else {
      return ch;
   }
}

/** Determine if a character is a valid for a shell variable. */
char IsSymbolic(char ch)
{
   /* This should work in either ASCII or EBCDIC. */
   if(ch >= 'A' && ch <= 'I') {
      return 1;
   } else if(ch >= 'J' && ch <= 'R') {
      return 1;
   } else if(ch >= 'S' && ch <= 'Z') {
      return 1;
   } else if(ch >= 'a' && ch <= 'i') {
      return 1;
   } else if(ch >= 'j' && ch <= 'r') {
      return 1;
   } else if(ch >= 's' && ch <= 'z') {
      return 1;
   } else if(ch >= '0' && ch <= '9') {
      return 1;
   } else if(ch == '_') {
      return 1;
   } else {
      return 0;
   }
}

/** Get the name of a shell variable (returns a copy). */
char *GetSymbolName(const char *str)
{

   char *temp;

   if(*str == '$') {
      temp = Allocate(2);
      temp[0] = '$';
      temp[1] = 0;
   } else {
      int stop;
      for(stop = 0; IsSymbolic(str[stop]); stop++);
      temp = Allocate(stop + 1);
      memcpy(temp, str, stop);
      temp[stop] = 0;
   }

   return temp;

}

/** Replace "name" with "value" at offset in str (reallocates if needed). */
void ReplaceSymbol(char **str, unsigned int offset,
                   const char *name, const char *value)
{

   char *temp;
   int strLength;
   int nameLength;
   int valueLength;

   Assert(str);
   Assert(name);

   /* Determine string lengths. */
   strLength = strlen(*str);
   nameLength = strlen(name) + 1; /* Account for the '$'. */
   if(value) {
      valueLength = strlen(value);
   } else {
      valueLength = 0;
   }

   /* Allocate extra space if necessary. */
   if(valueLength > nameLength) {
		const size_t totalLen = strLength - nameLength + valueLength + 1;
      temp = Allocate(totalLen);
      memcpy(temp, *str, strLength + 1);
      Release(*str);
      *str = temp;
   }

   /* Offset to '$'. */
   temp = *str + offset;

   if(nameLength > valueLength) {

      /* Move left */
      memmove(temp, temp + nameLength - valueLength,
              strLength - offset - nameLength + valueLength + 1);

   } else if(nameLength < valueLength) {

      /* Move right */
      memmove(temp + valueLength, temp + nameLength,
              strLength + - nameLength - offset + 1);

   }

   if(value) {
      memcpy(temp, value, valueLength);
   }

}

/** Perform shell-like macro path expansion. */
void ExpandPath(char **path)
{

   char *name;
   char *value;
   unsigned int x;

   Assert(path);

   for(x = 0; (*path)[x]; x++) {

      if((*path)[x] == '$') {
         name = GetSymbolName(*path + x + 1);
         value = getenv(name);
         ReplaceSymbol(path, x, name, value);
         Release(name);
         if(value) {
            x += strlen(value) - 1;
         }
      }

   }

}

/** Trim leading and trailing whitespace from a string. */
void Trim(char *str)
{

   unsigned int length;
   unsigned int start;
   unsigned int x;
   unsigned int line;

   Assert(str);

   length = strlen(str);

   /* Determine how much to cut off of the left. */
   line = 0;
   for(start = 0; IsSpace(str[start], &line); start++);

   /* Trim the left. */
   if(start > 0) {
      length -= start;
      for(x = 0; x < length + 1; x++) {
         str[x] = str[x + start];
      }
   }

   /* Trim the right. */
   while(length > 0 && IsSpace(str[length - 1], &line)) {
      length -= 1;
      str[length] = 0;
   }

}

/** Copy a string. */
char *CopyString(const char *str)
{

   char *temp;
   unsigned int len;

   if(!str) {
      return NULL;
   }

   len = strlen(str) + 1;
   temp = Allocate(len);
   memcpy(temp, str, len);

   return temp;

}

/** Parse a float. */
float ParseFloat(const char *str)
{
   float result;
#if defined(HAVE_SETLOCALE) && (defined(ENABLE_NLS) || defined(ENABLE_ICONV))
   setlocale(LC_ALL, "C");
#endif
   result = atof(str);
#if defined(HAVE_SETLOCALE) && (defined(ENABLE_NLS) || defined(ENABLE_ICONV))
   setlocale(LC_ALL, "");
#endif
   return result;
}

/** Find a value in a string mapping. */
int FindValue(const StringMappingType *mapping, int count, const char *key)
{
   int left = 0;
   int right = count - 1;
   while(right >= left) {
      const int x = (left + right) / 2;
      Assert(x >= 0);
      Assert(x < count);
      const int rc = strcmp(key, mapping[x].key);
      if(rc < 0) {
         right = x - 1;
      } else if(rc > 0) {
         left = x + 1;
      } else {
         return mapping[x].value;
      }
   }
   return -1;
}

/** Find a key in a string mapping. */
const char *FindKey(const StringMappingType *mapping, int count, int value)
{
   int x;
   for(x = 0; x < count; x++) {
      if(mapping[x].value == value) {
         return mapping[x].key;
      }
   }
   return NULL;
}

/** Case insensitive string compare. */
int StrCmpNoCase(const char *a, const char *b)
{
   while(*a && *b && ToLower(*a) == ToLower(*b)) {
      a += 1;
      b += 1;
   }
   return *b - *a;
}
