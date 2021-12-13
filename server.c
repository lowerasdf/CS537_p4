#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include "udp.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)

#define ROOT 0

#define MAX_INODES 4096
#define MAX_FILE_BLOCKS 14
#define DIR_ENTRY_SIZE 32
#define MAX_NUM_DIR_ENTRY_PER_BLOCK (MFS_BLOCK_SIZE / DIR_ENTRY_SIZE)
#define MAX_FILES_PER_DIR (MAX_FILE_BLOCKS * MAX_NUM_DIR_ENTRY_PER_BLOCK) - 2
#define MAX_NAME_LEN 28

#define MAX_INODES_MAP_ENTRIES 16
#define MAX_NUM_INODES_PIECES (MAX_INODES / MAX_INODES_MAP_ENTRIES)

int fd;
int sd;
char *filename;
int portnum;

struct INode {
    MFS_Stat_t stat;
    int ptr[MAX_FILE_BLOCKS];
};

// struct INodeMap {
//     int map[MAX_INODES_MAP_ENTRIES];
// };

struct CheckpointRegion {
    int endOfLog;
    int iNodeMapPtr[MAX_NUM_INODES_PIECES];
} CR;

// struct CheckpointRegionInMemory {
//     int endOfLog;
//     struct INode iNodePiecesMap[MAX_NUM_INODES_PIECES][MAX_INODES_MAP_ENTRIES];
// };

int cached_map[MAX_NUM_INODES_PIECES][MAX_INODES_MAP_ENTRIES];

void gracefulExit(int sig_num) {
    close(fd);
    exit(0);
}

int isNumber(char *s) {
    if (s[0] == '\0') {
        return 0;
    }
    for (int i = 0; s[i] != '\0'; i++){
        if (isdigit(s[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

int parseArgs(int argc, char *argv[]) {
    if (argc != 3) {
        return -1;
    }
    if (!isNumber(argv[1])) {
        return -1;
    }
    
    filename = argv[2];
    portnum = atoi(argv[1]);

    return 0;
}

void setup() {
    sd = UDP_Open(portnum);
    if (sd < 0) {
        fprintf(stderr, "server:: cannot open port %d\n", portnum);
        exit(1);
    }

    fd = open(filename, O_RDWR);
    if (fd < 0) {
        if (errno != ENOENT) {
            fprintf(stderr, "server:: cannot open file %s\n", filename);
            exit(1);
        } else {
            fd = open(filename, O_RDWR | O_CREAT);

            // Initialize root dir with . and ..
            MFS_DirEnt_t root_data[MAX_NUM_DIR_ENTRY_PER_BLOCK];
            strcpy(root_data[0].name, ".");
            root_data[0].inum = ROOT;
            strcpy(root_data[1].name, "..");
            root_data[1].inum = ROOT;
            for (int i = 2; i < MAX_NUM_DIR_ENTRY_PER_BLOCK; i++) {
                root_data[i].inum = -1;
            }

            // Initialize inode for root
            MFS_Stat_t root_stat;
            root_stat.type = MFS_DIRECTORY;
            root_stat.size = 2 * DIR_ENTRY_SIZE;
            struct INode root_inode;
            root_inode.stat = root_stat;
            root_inode.ptr[0] = sizeof(struct CheckpointRegion);

            // Initialize inode map
            int first_inode_map[MAX_INODES_MAP_ENTRIES];
            first_inode_map[0] = sizeof(struct CheckpointRegion) + MFS_BLOCK_SIZE;
            cached_map[0][0] = sizeof(struct CheckpointRegion) + MFS_BLOCK_SIZE;
            for (int i = 1; i < MAX_INODES_MAP_ENTRIES; i++) {
                first_inode_map[i] = -1;
                cached_map[0][i] = 0;
            }

            // Initialize CR
            CR.iNodeMapPtr[0] = sizeof(struct CheckpointRegion) + (2 * MFS_BLOCK_SIZE);
            CR.endOfLog = sizeof(struct CheckpointRegion) + (3 * MFS_BLOCK_SIZE);

            // Clear pointers of pieces of INodes maps
            for (int i = 1; i < MAX_NUM_INODES_PIECES; i++) {
                CR.iNodeMapPtr[i] = -1;  
            }

            lseek(fd, 0, SEEK_SET);
            int n_write = write(fd, &CR, sizeof(struct CheckpointRegion));
            if (n_write < 0) {
                fprintf(stderr, "server:: cannot write to file\n");
                exit(1);
            }

            lseek(fd, sizeof(struct CheckpointRegion), SEEK_SET);
            n_write = write(fd, &root_data, MFS_BLOCK_SIZE);
            if (n_write < 0) {
                fprintf(stderr, "server:: cannot write to file\n");
                exit(1);
            }

            lseek(fd, sizeof(struct CheckpointRegion) + MFS_BLOCK_SIZE, SEEK_SET);
            n_write = write(fd, &root_inode, MFS_BLOCK_SIZE);
            if (n_write < 0) {
                fprintf(stderr, "server:: cannot write to file\n");
                exit(1);
            }

            lseek(fd, sizeof(struct CheckpointRegion) + (2 * MFS_BLOCK_SIZE), SEEK_SET);
            n_write = write(fd, &first_inode_map, MFS_BLOCK_SIZE);
            if (n_write < 0) {
                fprintf(stderr, "server:: cannot write to file\n");
                exit(1);
            }

            int fsync_status = fsync(fd);
            if (fsync_status < 0) {
                fprintf(stderr, "server:: cannot fsync to file\n");
                exit(1);
            }
        }
    } else {
        lseek(fd, 0, SEEK_SET);
        int n_read = read(fd, &CR, sizeof(struct CheckpointRegion));
        if (n_read < 0) {
            fprintf(stderr, "server:: cannot read CR from the existing file\n");
            exit(1);
        }

        for (int i = 0; i < MAX_NUM_INODES_PIECES; i++) {
            int ptr = CR.iNodeMapPtr[i];
            if (ptr == -1) {
                continue;
            }

            int currINodeMap[MAX_INODES_MAP_ENTRIES];

            lseek(fd, ptr, SEEK_SET);
            n_read = read(fd, &currINodeMap, MFS_BLOCK_SIZE);
            if (n_read < 0) {
                fprintf(stderr, "server:: cannot read inode map from the existing file\n");
                exit(1);
            }

            for (int j = 0; j < MAX_INODES_MAP_ENTRIES; j++) {
                if (currINodeMap[j] != -1) {
                    cached_map[i][j] = currINodeMap[j];
                } else {
                    cached_map[i][j] = 0;
                }
            }
        }
    }

    signal(SIGINT, gracefulExit);
}

int main(int argc, char *argv[]) { 
    if (parseArgs(argc, argv) != 0) {
        fprintf(stderr, "usage: server [portnum] [file-system-image]\n");
        exit(1);
    }

    setup();

    // for (int i = 0; i < MAX_NUM_INODES_PIECES; i++) {
    //     for (int j = 0; j < MAX_INODES_MAP_ENTRIES; j++) {
    //         if (cached_map[i][j] != 0) {
    //             printf("[%d,%d] address:%d\n", i, j, cached_map[i][j]);
    //         }
    //     }
    // }

    while (1) {
        struct sockaddr_in addr;
        char message[BUFFER_SIZE];
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
        printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
        if (rc > 0) {
                char reply[BUFFER_SIZE];
                sprintf(reply, "goodbye world");
                rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            printf("server:: reply\n");
        } 
    }
    
    return 0; 
}
    
    // struct CheckpointRegion CR_TEST;
    // MFS_DirEnt_t root_data_TEST[MAX_NUM_DIR_ENTRY_PER_BLOCK];
    // struct INode root_inode_TEST;
    // int first_inode_map_TEST[MAX_INODES_MAP_ENTRIES];

    // lseek(fd, 0, SEEK_SET);
    // read(fd, &CR_TEST, sizeof(struct CheckpointRegion));
    // printf("[CR] endOfLog: %d, [%d, %d, %d, %d]\n", CR_TEST.endOfLog, CR_TEST.iNodeMapPtr[0], CR_TEST.iNodeMapPtr[1], CR_TEST.iNodeMapPtr[2], CR_TEST.iNodeMapPtr[3]);

    // lseek(fd, sizeof(struct CheckpointRegion), SEEK_SET);
    // read(fd, &root_data_TEST, MFS_BLOCK_SIZE);
    // printf("[Root Data] dir1.name:%s, dir1.inum:%d, dir2.name:%s, dir2.inum:%d\n", root_data_TEST[0].name, root_data_TEST[0].inum, root_data_TEST[1].name, root_data_TEST[1].inum);

    // lseek(fd, sizeof(struct CheckpointRegion) + MFS_BLOCK_SIZE, SEEK_SET);
    // read(fd, &root_inode_TEST, MFS_BLOCK_SIZE);
    // printf("[Root INode] type:%d, size:%d, ptr1:%d\n", root_inode_TEST.stat.type, root_inode_TEST.stat.size, root_inode_TEST.ptr[0]);

    // lseek(fd, sizeof(struct CheckpointRegion) + (2 * MFS_BLOCK_SIZE), SEEK_SET);
    // read(fd, &first_inode_map_TEST, MFS_BLOCK_SIZE);
    // printf("[First INode Map] ptr1:%d, ptr2:%d, ptr3:%d, ptr4:%d\n", first_inode_map_TEST[0], first_inode_map_TEST[1], first_inode_map_TEST[2], first_inode_map_TEST[3]);


    // struct CheckpointRegion CR;
    // CR.endOfLog = 10;
    // CR.iNodeMapPtr[0] = 1;
    // CR.iNodeMapPtr[1] = 5;
    // CR.iNodeMapPtr[3] = 10;
    // printf("endOfLog: %d, [%d, %d, %d, %d]\n", CR.endOfLog, CR.iNodeMapPtr[0], CR.iNodeMapPtr[1], CR.iNodeMapPtr[2], CR.iNodeMapPtr[3]);
    // off_t off = lseek(fd, 0, SEEK_SET);
    // printf("off: %d\n", (int) off);
    // int n_write = write(fd, &CR, sizeof(struct CheckpointRegion));
    // printf("n_write: %d, err_no:%d, size:%ld\n", n_write, errno, sizeof(struct CheckpointRegion));

    // struct CheckpointRegion result;
    // result.endOfLog = 5;
    // printf("endOfLog: %d, [%d, %d, %d, %d]\n", result.endOfLog, result.iNodeMapPtr[0], result.iNodeMapPtr[1], result.iNodeMapPtr[2], result.iNodeMapPtr[3]);
    // lseek(fd, 0, SEEK_SET);
    // int n_read = read(fd, &result, sizeof(struct CheckpointRegion));
    // printf("n_read: %d, err_no:%d, size:%ld\n", n_read, errno, sizeof(struct CheckpointRegion));
    // printf("endOfLog: %d, [%d, %d, %d, %d]\n", result.endOfLog, result.iNodeMapPtr[0], result.iNodeMapPtr[1], result.iNodeMapPtr[2], result.iNodeMapPtr[3]);