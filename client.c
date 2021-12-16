#include <stdio.h>
#include <string.h>
#include "mfs.h"

void test_read_write() {
    int lookup_status = MFS_Lookup(0, ".");
    printf("lookup_status:%d\n", lookup_status);

    MFS_Stat_t stat;
    int stat_status = MFS_Stat(0, &stat);
    printf("stat_status:%d, stat.size:%d, stat.type:%d\n", stat_status, stat.size, stat.type);

    int creat_status = MFS_Creat(0, MFS_REGULAR_FILE, "file_1");
    printf("creat_status:%d\n", creat_status);

    // int creat_status = MFS_Creat(0, MFS_DIRECTORY, "dir_1");
    // printf("creat_status:%d\n", creat_status);

    int child_inum = MFS_Lookup(0, "file_1");
    printf("child_inum:%d\n", child_inum);

    int write_status = MFS_Write(child_inum, "this is a file", 0);
    printf("write_status:%d\n", write_status);

    char buffer[MFS_BLOCK_SIZE];
    int read_status = MFS_Read(child_inum, buffer, 0);
    printf("read_status:%d\n", read_status);
    printf("read_data:%s\n", buffer);
    // MFS_DirEnt_t directory_entry[128];
    // memcpy(directory_entry, buffer, sizeof(MFS_DirEnt_t) * 128);
    // for(int i = 0; i < 128; i++) {
    //     printf("%s:%d\n", directory_entry[i].name, directory_entry[i].inum);
    // }

    int unlink_status = MFS_Unlink(0, "file_1");
    printf("unlink_status:%d\n", unlink_status);

    // int shutdown_status = MFS_Shutdown();
    // printf("shutdown_status:%d\n", shutdown_status);
}

void test_non_empty() {
    int creat_status = MFS_Creat(0, MFS_DIRECTORY, "dir_1");
    printf("creat_status:%d\n", creat_status);

    int child_inum = MFS_Lookup(0, "dir_1");
    printf("child_inum:%d\n", child_inum);

    creat_status = MFS_Creat(child_inum, MFS_REGULAR_FILE, "file_1");
    printf("creat_status:%d\n", creat_status);

    int file_inum = MFS_Lookup(child_inum, "file_1");
    printf("file_inum:%d\n", file_inum);

    int unlink_status = MFS_Unlink(0, "dir_1");
    printf("dir unlink_status:%d\n", unlink_status);

    unlink_status = MFS_Unlink(child_inum, "file_1");
    printf("file unlink_status:%d\n", unlink_status);

    unlink_status = MFS_Unlink(0, "dir_1");
    printf("dir unlink_status:%d\n", unlink_status);
}

int main(int argc, char *argv[]) {
    int rc = MFS_Init("localhost", 10000);
    printf("rc %d\n", rc);

    test_non_empty();

    return 0;
}
