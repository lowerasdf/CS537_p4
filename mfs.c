#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include "mfs.h"
#include "udp.h"

int sd, rc;
struct sockaddr_in addrSnd, addrRcv;

int MFS_Init(char *hostname, int port) {
    sd = UDP_Open(20010);
    if (sd < 0) {
        return -1;
    }

    printf("sd:%d\n", sd);

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

    // Send request
    rc = UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        return -1;
    }

    // Wait response
    int status;
    rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));

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
    
    // Send request
    rc = UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        return -1;
    }

    // Wait response
    MFS_Stat_Response response;
    UDP_Read(sd, &addrRcv, (char *) &response, sizeof(MFS_Stat_Response));

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
    
    // Send request
    rc = UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        return -1;
    }

    // Wait response
    int status;
    rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));

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
    
    // Send request
    rc = UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        return -1;
    }

    // Wait response
    MFS_Read_Response response;
    rc = UDP_Read(sd, &addrRcv, (char*) &response, sizeof(MFS_Read_Response));

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

    // Send request
    rc = UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        return -1;
    }

    // Wait response
    int status;
    rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));

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

    // Send request
    rc = UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        return -1;
    }

    // Wait response
    int status;
    rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));

    return status;
}

int MFS_Shutdown() {
    if (sd < 0) {
        return -1;
    }
    
    // Create request param
    MFS_Shutdown_Request request;
    request.method = ShutDown;

    // Send request
    rc = UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        return -1;
    }

    // Wait response
    int status;
    rc = UDP_Read(sd, &addrRcv, (char *) &status, sizeof(int));

    return status;
}
