#ifndef HAVE_DEVPROC_H
#define HAVE_DEVPROC_H

struct linktest_value {
        unsigned int    cmd;
        unsigned int    data;
};

FILE *netproc_open(void);
void netproc_close(FILE *fp);
int netproc_netdevlist_clear(NETDEVLIST_PTR *netdev_list);
int netproc_scandevice(int sockfd, int iwsockfd, FILE *fp, NETDEVLIST_PTR *netdev_list);
void netproc_print(NETDEVLIST_PTR netdev_list);
void netproc_listener(FNETD *fnetd);
void netproc_devicelist_clear(NETDEVLIST_PTR *netdev_list);

#endif
