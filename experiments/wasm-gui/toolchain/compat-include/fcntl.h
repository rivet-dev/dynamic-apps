#ifndef SE_COMPAT_FCNTL_H
#define SE_COMPAT_FCNTL_H
#include_next <fcntl.h>
#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC 1030
#endif
#endif
