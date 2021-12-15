#include <stdio.h>
#include "mfs.h"

int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 10000);
    printf("rc %d\n", rc);

    int lookup_status = MFS_Lookup(0, ".");
    printf("lookup_status:%d\n", lookup_status);

    MFS_Stat_t stat;
    int stat_status = MFS_Stat(0, &stat);
    printf("stat_status:%d, stat.size:%d, stat.type:%d\n", stat_status, stat.size, stat.type);

    int creat_status = MFS_Creat(0, MFS_REGULAR_FILE, "file_1");
    printf("creat_status:%d\n", creat_status);

    int child_inum = MFS_Lookup(0, "file_1");
    printf("child_inum:%d\n", child_inum);

    int write_status = MFS_Write(child_inum, "this is a file\nhello world", 0);
    printf("write_status:%d\n", write_status);

    char buffer[MFS_BLOCK_SIZE];
    int read_status = MFS_Read(child_inum, buffer, 0);
    printf("read_status:%d\n", read_status);
    printf("read_data:%s\n", buffer);

    int unlink_status = MFS_Unlink(0, "file_1");
    printf("unlink_status:%d\n", unlink_status);

    // int shutdown_status = MFS_Shutdown();
    // printf("shutdown_status:%d\n", shutdown_status);

    return 0;
}
