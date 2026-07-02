#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <sys/ioctl.h>

#ifndef FIONREAD
#define FIONREAD 0x541B
#endif

#define HOST_NET_FD_BASE 0x40000000

static int is_host_net_fd(int fd) {
    return fd >= HOST_NET_FD_BASE;
}

int ioctl(int fd, int request, ...) {
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);

    if (request == FIONREAD && is_host_net_fd(fd)) {
        struct pollfd pfd = {
            .fd = fd,
            .events = POLLIN,
            .revents = 0,
        };
        int ready = poll(&pfd, 1, 0);
        if (ready < 0) {
            return -1;
        }
        if (arg) {
            *(int *)arg = ready > 0 && (pfd.revents & POLLIN) ? 1 : 0;
        }
        return 0;
    }

    errno = ENOTTY;
    return -1;
}
