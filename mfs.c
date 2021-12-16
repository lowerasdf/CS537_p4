#include <stdio.h>
#include "mfs.h"
#include "udp.h"

int sd, rc;
struct sockaddr_in addrSnd, addrRcv;

int MFS_Init(char *hostname, int port) {
    sd = UDP_Open(20000);
    if (sd < 0) {
        return -1;
    }

    rc = UDP_FillSockAddr(&addrSnd, hostname, port);
    return 0;
}

int MFS_Lookup(int pinum, char *name) {
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
    char response_buffer[BUFFER_SIZE];
    rc = UDP_Read(sd, &addrRcv, response_buffer, sizeof(char) * BUFFER_SIZE);
    memcpy(&status, response_buffer, sizeof(int));

    return status;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
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
    char response_buffer[BUFFER_SIZE];
    rc = UDP_Read(sd, &addrRcv, response_buffer, sizeof(char) * BUFFER_SIZE);
    memcpy(&response, response_buffer, sizeof(MFS_Stat_Response));

    m->type = response.stat.type;
    m->size = response.stat.size;

    return response.status;
}

int MFS_Write(int inum, char *buffer, int block) {
    // Create request param
    MFS_Write_Request request;
    request.method = Write;
    request.inum = inum;
    strcpy(request.buffer, buffer);
    request.block = block;
    
    // Send request
    rc = UDP_Write(sd, &addrSnd, (char *) &request, BUFFER_SIZE);
    if (rc < 0) {
        printf("client:: failed to send\n");
        return -1;
    }

    // Wait response
    int status;
    char response_buffer[BUFFER_SIZE];
    rc = UDP_Read(sd, &addrRcv, response_buffer, sizeof(char) * BUFFER_SIZE);
    memcpy(&status, response_buffer, sizeof(int));

    return status;
}

int MFS_Read(int inum, char *buffer, int block) {
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
    char response_buffer[BUFFER_SIZE];
    rc = UDP_Read(sd, &addrRcv, response_buffer, sizeof(char) * BUFFER_SIZE);
    memcpy(&response, response_buffer, sizeof(MFS_Read_Response));
    strcpy(buffer, response.buffer);

    return response.status;
}

int MFS_Creat(int pinum, int type, char *name) {
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
    char response_buffer[BUFFER_SIZE];
    rc = UDP_Read(sd, &addrRcv, response_buffer, sizeof(char) * BUFFER_SIZE);
    memcpy(&status, response_buffer, sizeof(int));

    return status;
}

int MFS_Unlink(int pinum, char *name) {
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
    char response_buffer[BUFFER_SIZE];
    rc = UDP_Read(sd, &addrRcv, response_buffer, sizeof(char) * BUFFER_SIZE);
    memcpy(&status, response_buffer, sizeof(int));

    return status;
}

int MFS_Shutdown() {
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
    char response_buffer[BUFFER_SIZE];
    rc = UDP_Read(sd, &addrRcv, response_buffer, sizeof(char) * BUFFER_SIZE);
    memcpy(&status, response_buffer, sizeof(int));

    return status;
}