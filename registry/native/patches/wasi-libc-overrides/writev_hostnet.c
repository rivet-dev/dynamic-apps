#include <errno.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#define HOST_NET_FD_BASE 0x40000000

extern ssize_t __real_writev(int fd, const struct iovec *iov, int iovcnt);

static int is_host_net_fd(int fd) {
    return fd >= HOST_NET_FD_BASE;
}

ssize_t __wrap_writev(int fd, const struct iovec *iov, int iovcnt) {
    if (!is_host_net_fd(fd)) {
        return __real_writev(fd, iov, iovcnt);
    }

    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        const char *p = (const char *)iov[i].iov_base;
        size_t left = iov[i].iov_len;
        while (left > 0) {
            ssize_t n = send(fd, p, left, 0);
            if (n < 0) {
                if (total > 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    return total;
                }
                return -1;
            }
            if (n == 0) {
                return total;
            }
            total += n;
            p += n;
            left -= (size_t)n;
        }
    }
    return total;
}
