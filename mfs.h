#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE   (4096)

#define BUFFER_SIZE (8192)

typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t {
    char name[28];  // up to 28 bytes of name in directory (including \0)
    int  inum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;

typedef enum {Lookup, Stat, Write, Read, Creat, Unlink, ShutDown} Method;

typedef struct __MFS_Lookup_Request {
    Method method;
    int pinum;
    char name[1024];
} MFS_Lookup_Request;

typedef struct __MFS_Stat_Request {
    Method method;
    int inum;
} MFS_Stat_Request;

typedef struct __MFS_Write_Request {
    Method method;
    int inum;
    char buffer[MFS_BLOCK_SIZE];
    int block;
} MFS_Write_Request;

typedef struct __MFS_Read_Request {
    Method method;
    int inum;
    int block;
} MFS_Read_Request;

typedef struct __MFS_Creat_Request {
    Method method;
    int pinum;
    int type;
    char name[1024];
} MFS_Creat_Request;

typedef struct __MFS_Unlink_Request {
    Method method;
    int pinum;
    char name[1024];
} MFS_Unlink_Request;

typedef struct __MFS_Shutdown_Request {
    Method method;
} MFS_Shutdown_Request;

typedef struct __MFS_Stat_Response {
    int status;
    MFS_Stat_t stat;
} MFS_Stat_Response;

typedef struct __MFS_Read_Response {
    int status;
    char buffer[MFS_BLOCK_SIZE];
} MFS_Read_Response;

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

#endif // __MFS_h__
