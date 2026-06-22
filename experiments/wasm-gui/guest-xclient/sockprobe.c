#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
extern unsigned __secure_exec_net_set_nonblock(unsigned fd, unsigned en)
    __attribute__((import_module("host_net"), import_name("net_set_nonblock")));
static void m(const char*s){ fprintf(stderr,"%s",s); fflush(stderr); }
int main(void){
    m("SP:start\n");
    fprintf(stderr,"SP:EAGAIN=%d EWOULDBLOCK=%d\n", EAGAIN, EWOULDBLOCK); fflush(stderr);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un a; memset(&a,0,sizeof a); a.sun_family=AF_UNIX;
    strcpy(a.sun_path,"/tmp/.X11-unix/X0");
    connect(fd,(struct sockaddr*)&a,sizeof a);
    unsigned nb = __secure_exec_net_set_nonblock((unsigned)fd, 1);
    fprintf(stderr,"SP:net_set_nonblock ret=%u\n", nb); fflush(stderr);
    /* recv on empty non-blocking socket (no data sent yet) */
    char b[8]; errno=0; long r1 = recv(fd, b, 8, 0);
    fprintf(stderr,"SP:recv_empty r=%ld errno=%d (EAGAIN?%d)\n", r1, errno, errno==EAGAIN); fflush(stderr);
    /* now send the setup, poll, recv */
    unsigned char req[12]={0}; req[0]='l'; req[2]=11; send(fd,req,12,0);
    struct pollfd pf={ .fd=fd, .events=POLLIN }; poll(&pf,1,2000);
    errno=0; long r2 = recv(fd, b, 8, 0);
    fprintf(stderr,"SP:recv_after r=%ld errno=%d resp0=%d\n", r2, errno, (unsigned char)b[0]); fflush(stderr);
    m("SP:done\n");
    return 0;
}
