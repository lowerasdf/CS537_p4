#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include "mfs.h"
#include "udp.h"

int sd, rc;
struct sockaddr_in addrSnd, addrRcv;

int MFS_Init(char *hostname, int port) {
    for (int i = 0; i < 10000; i++) {
        sd = UDP_Open(20000 + i);
        if (sd >= 0) {
            break;
        }
    }
    if (sd < 0) {
        return -1;
    }

    rc = UDP_FillSockAddr(&addrSnd, hostname, port);
    return 0;
}

int MFS_Lookup(int pinum, char *name) {
    if (sd < 0) {
        return -1;
    }
    
    // Create request param
    MFS_Lookup_Request request;
    request.method = Lookup;
    request.pinum = pinum;
    strcpy(request.name, name);

    int ready = 0;
    fd_set rfds;
    struct timeval tv;

    int status = -1;

    do {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        // Send request
        UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
        ready = select(sd+1, &rfds, NULL, NULL, &tv);
        if (ready == -1) {
            return -1;
        } else if (ready) {
            // Wait response
            rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));
        }
    } while(!ready);

    return status;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    if (sd < 0) {
        return -1;
    }
    
    // Create request param
    MFS_Stat_Request request;
    request.method = Stat;
    request.inum = inum;

    int ready = 0;
    fd_set rfds;
    struct timeval tv;

    MFS_Stat_Response response;

    do {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        // Send request
        UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
        ready = select(sd+1, &rfds, NULL, NULL, &tv);
        if (ready == -1) {
            return -1;
        } else if (ready) {
            // Wait response
            rc = UDP_Read(sd, &addrRcv, (char *) &response, sizeof(MFS_Stat_Response));
        }
    } while(!ready);

    m->type = response.stat.type;
    m->size = response.stat.size;

    return response.status;
}

int MFS_Write(int inum, char *buffer, int block) {
    if (sd < 0) {
        return -1;
    }
    
    // Create request param
    MFS_Write_Request request;
    request.method = Write;
    request.inum = inum;
    for(int i = 0; i < MFS_BLOCK_SIZE; i++) {
        request.buffer[i]=buffer[i];
    }
    request.block = block;

    int ready = 0;
    fd_set rfds;
    struct timeval tv;

    int status = -1;

    do {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        // Send request
        UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
        ready = select(sd+1, &rfds, NULL, NULL, &tv);
        if (ready == -1) {
            return -1;
        } else if (ready) {
            // Wait response
            rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));
        }
    } while(!ready);

    return status;
}

int MFS_Read(int inum, char *buffer, int block) {
    if (sd < 0) {
        return -1;
    }
    
    // Create request param
    MFS_Read_Request request;
    request.method = Read;
    request.inum = inum;
    request.block = block;
    
    int ready = 0;
    fd_set rfds;
    struct timeval tv;

    MFS_Read_Response response;

    do {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        // Send request
        UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
        ready = select(sd+1, &rfds, NULL, NULL, &tv);
        if (ready == -1) {
            return -1;
        } else if (ready) {
            // Wait response
            rc = UDP_Read(sd, &addrRcv, (char*) &response, sizeof(MFS_Read_Response));
        }
    } while(!ready);

    if (rc > -1) {
        for (int i = 0; i < MFS_BLOCK_SIZE; i++) {
            buffer[i] = response.buffer[i];
        }
    }

    return response.status;
}

int MFS_Creat(int pinum, int type, char *name) {
    if (sd < 0) {
        return -1;
    }
    
    // Create request param
    MFS_Creat_Request request;
    request.method = Creat;
    request.pinum = pinum;
    request.type = type;
    strcpy(request.name, name);

    int ready = 0;
    fd_set rfds;
    struct timeval tv;

    int status = -1;

    do {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        // Send request
        UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
        ready = select(sd+1, &rfds, NULL, NULL, &tv);
        if (ready == -1) {
            return -1;
        } else if (ready) {
            // Wait response
            rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));
        }
    } while(!ready);

    return status;
}

int MFS_Unlink(int pinum, char *name) {
    if (sd < 0) {
        return -1;
    }
    
    // Create request param
    MFS_Unlink_Request request;
    request.method = Unlink;
    request.pinum = pinum;
    strcpy(request.name, name);

    int ready = 0;
    fd_set rfds;
    struct timeval tv;

    int status = -1;

    do {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        // Send request
        UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
        ready = select(sd+1, &rfds, NULL, NULL, &tv);
        if (ready == -1) {
            return -1;
        } else if (ready) {
            // Wait response
            rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));
        }
    } while(!ready);

    return status;
}

int MFS_Shutdown() {
    if (sd < 0) {
        return -1;
    }
    
    // Create request param
    MFS_Shutdown_Request request;
    request.method = ShutDown;

    int ready = 0;
    fd_set rfds;
    struct timeval tv;

    int status = -1;

    do {
        FD_ZERO(&rfds);
        FD_SET(sd, &rfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        
        // Send request
        UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
        ready = select(sd+1, &rfds, NULL, NULL, &tv);
        if (ready == -1) {
            return -1;
        } else if (ready) {
            // Wait response
            rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));
        }
    } while(!ready);

    return status;
}
