#ifndef SE_COMPAT_STRING_H
#define SE_COMPAT_STRING_H
#include_next <string.h>
#ifdef __cplusplus
extern "C" {
#endif
char *strchrnul(const char *, int);
void explicit_bzero(void *, size_t);
#ifdef __cplusplus
}
#endif
#endif
