#include <stdio.h>
#include "mfs.h"
#include "udp.h"

int sd, rc;
struct sockaddr_in addrSnd, addrRcv;

int MFS_Init(char *hostname, int port) {
    // printf("init %s %d\n", hostname, port);
    sd = UDP_Open(port);
    if (sd < 0) {
        return -1;
    }
    rc = UDP_FillSockAddr(&addrSnd, hostname, port);
    // printf("sd: %d, rc: %d\n", sd, rc);
    return 0;
}

int MFS_Lookup(int pinum, char *name) {
    return 0;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    return 0;
}

int MFS_Write(int inum, char *buffer, int block) {
    return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
    return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
    return 0;
}

int MFS_Unlink(int pinum, char *name) {
    return 0;
}

int MFS_Shutdown() {
    return 0;
}