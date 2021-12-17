#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include "udp.h"
#include "mfs.h"

#define ROOT 0

#define MAX_INODES 4096
#define MAX_FILE_BLOCKS 14
#define DIR_ENTRY_SIZE 32
#define MAX_NUM_DIR_ENTRY_PER_BLOCK (MFS_BLOCK_SIZE / DIR_ENTRY_SIZE)
#define MAX_FILES_PER_DIR (MAX_FILE_BLOCKS * MAX_NUM_DIR_ENTRY_PER_BLOCK) - 2
#define MAX_NAME_LEN 27

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

struct CheckpointRegion {
    int endOfLog;
    int iNodeMapPtr[MAX_NUM_INODES_PIECES];
} CR;

int cached_map[MAX_NUM_INODES_PIECES][MAX_INODES_MAP_ENTRIES];

void shutDown() {
    int fsync_status = fsync(fd);
    if (fsync_status < 0) {
        fprintf(stderr, "server:: cannot fsync to file\n");
        exit(1);
    }
    close(fd);
    exit(0);
}

void gracefulExit(int sig_num) {
    shutDown();
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

void getPieceNoAndOffset(int inum, int *pieces_no, int *offset) {
    *pieces_no = (int) ((float) inum / MAX_INODES_MAP_ENTRIES);
    *offset = 0;
    if (inum != 0) {
        *offset = inum % MAX_INODES_MAP_ENTRIES;
    }
}

int getAddressOrError(int inum) {
    if (inum >= MAX_INODES || inum < 0) {
        return -1;
    }

    int pieces_no;
    int offset;
    getPieceNoAndOffset(inum, &pieces_no, &offset);

    return cached_map[pieces_no][offset];
}

int mfs_lookup(int pinum, char *name) {
    int disk_address = getAddressOrError(pinum);
    if (disk_address == -1) {
        return -1;
    }

    struct INode inode;
    lseek(fd, disk_address, SEEK_SET);
    int n_read = read(fd, &inode, sizeof(struct INode));
    if (n_read < 0) {
        fprintf(stderr, "server:: lookup cannot read inode\n");
        exit(1);
    }

    if (inode.stat.type == MFS_REGULAR_FILE) {
        return -1;
    }

    for (int i = 0; i < MAX_FILE_BLOCKS; i++) {
        if (inode.ptr[i] != -1) {
            MFS_DirEnt_t directory_entry[MAX_NUM_DIR_ENTRY_PER_BLOCK];
            lseek(fd, inode.ptr[i], SEEK_SET);
            n_read = read(fd, &directory_entry, sizeof(MFS_DirEnt_t) * MAX_NUM_DIR_ENTRY_PER_BLOCK);
            if (n_read < 0) {
                fprintf(stderr, "server:: lookup cannot read block\n");
                exit(1);
            }

            for (int j = 0; j < MAX_NUM_DIR_ENTRY_PER_BLOCK; j++) {
                if (directory_entry[j].inum != -1 && strcmp(directory_entry[j].name, name) == 0) {
                    return directory_entry[j].inum;
                }
            }
        }
    }

    return -1;
}

int mfs_stat(int inum, MFS_Stat_t *m) {
    int disk_address = getAddressOrError(inum);
    if (disk_address == -1) {
        return -1;
    }

    struct INode inode;
    lseek(fd, disk_address, SEEK_SET);
    int n_read = read(fd, &inode, sizeof(struct INode));
    if (n_read < 0) {
        fprintf(stderr, "server:: lookup cannot read inode\n");
        exit(1);
    }

    m->type = inode.stat.type;
    m->size = inode.stat.size;

    return 0;
}

int mfs_read(int inum, char *buffer, int block) {
    int disk_address = getAddressOrError(inum);
    if (disk_address == -1) {
        return -1;
    }

    if (block >= MAX_FILE_BLOCKS || block < 0) {
        return -1;
    }

    struct INode inode;
    lseek(fd, disk_address, SEEK_SET);
    int n_read = read(fd, &inode, sizeof(struct INode));
    if (n_read < 0) {
        fprintf(stderr, "server:: read cannot read inode\n");
        exit(1);
    }

    if (inode.ptr[block] == -1) {
        return -1;
    }

    lseek(fd, inode.ptr[block], SEEK_SET);
    int n_read_data = read(fd, buffer, MFS_BLOCK_SIZE);
    if (n_read_data < 0) {
        fprintf(stderr, "server:: read cannot read data\n");
        exit(1);
    }

    return 0;
}

int mfs_creat(int pinum, int type, char *name) {
    // Check if name has valid length
    if (strlen(name) > MAX_NAME_LEN) {
        return -1;
    }

    // Check if type is valid
    if (type != MFS_REGULAR_FILE && type != MFS_DIRECTORY) {
        return -1;
    }

    int disk_address = getAddressOrError(pinum);
    if (disk_address == -1) {
        return -1;
    }

    // Get parent's inode
    struct INode inode;
    lseek(fd, disk_address, SEEK_SET);
    int n_read = read(fd, &inode, sizeof(struct INode));
    if (n_read < 0) {
        fprintf(stderr, "server:: creat cannot read inode\n");
        exit(1);
    }

    // If parent is not a directory
    if (inode.stat.type != MFS_DIRECTORY) {
        return -1;
    }

    // Find free location
    int pieces_no, offset;
    for (pieces_no = 0; pieces_no < MAX_NUM_INODES_PIECES; pieces_no++) {
        int found = 0;
        for (offset = 0; offset < MAX_INODES_MAP_ENTRIES; offset++) {
            if (cached_map[pieces_no][offset] == -1) {
                found = 1;
                break;
            }
        }
        if (found) {
            break;
        }
    }

    // Inode full
    if (pieces_no == MAX_NUM_INODES_PIECES && offset == MAX_INODES_MAP_ENTRIES) {
        return -1;
    }

    // New inum for child
    int new_inum = (pieces_no * MAX_INODES_MAP_ENTRIES) + offset;

    // Parent address
    int parent_pieces_no;
    int parent_offset;
    getPieceNoAndOffset(pinum, &parent_pieces_no, &parent_offset);

    // Check if exists and determine the empty location in the parent directory
    int free_idx_block = 0;
    int free_idx_offset = 0;
    int free_block_found = 0;
    int free_offset_found = 0;
    for (int i = 0; i < MAX_FILE_BLOCKS; i++) {
        if (inode.ptr[i] != -1) {
            MFS_DirEnt_t directory_entry[MAX_NUM_DIR_ENTRY_PER_BLOCK];
            lseek(fd, inode.ptr[i], SEEK_SET);
            n_read = read(fd, &directory_entry, sizeof(MFS_DirEnt_t) * MAX_NUM_DIR_ENTRY_PER_BLOCK);
            if (n_read < 0) {
                fprintf(stderr, "server:: creat cannot read block\n");
                exit(1);
            }

            for (int j = 0; j < MAX_NUM_DIR_ENTRY_PER_BLOCK; j++) {
                if (directory_entry[j].inum != -1) {
                    if (strcmp(directory_entry[j].name, name) == 0) {
                        return 0;
                    }
                } else if (free_offset_found == 0 && free_block_found == 0){
                    free_idx_block = i;
                    free_idx_offset = j;
                    free_offset_found = 1;
                }
            }
        } else if (free_offset_found == 0 && free_block_found == 0){
            free_idx_block = i;
            free_block_found = 1;
        }
    }

    // Directory full
    if (free_offset_found == 0 && free_block_found == 0) {
        return -1;
    }

    // New disk addresses
    int ptr_child_data = CR.endOfLog;
    int ptr_child_inode = ptr_child_data;
    if (type == MFS_DIRECTORY) {
        ptr_child_inode += MFS_BLOCK_SIZE;
    }
    int ptr_parent_data = ptr_child_inode + MFS_BLOCK_SIZE;
    int ptr_parent_inode = ptr_parent_data + MFS_BLOCK_SIZE;
    int ptr_child_inode_map = ptr_parent_inode + MFS_BLOCK_SIZE;
    int ptr_parent_inode_map = ptr_child_inode_map + MFS_BLOCK_SIZE;
    int ptr_end_of_log = ptr_parent_inode_map;
    if (parent_pieces_no != pieces_no) {
        ptr_end_of_log += MFS_BLOCK_SIZE;
    }

    // Create child's data
    MFS_DirEnt_t child_data[MAX_NUM_DIR_ENTRY_PER_BLOCK];
    if (type == MFS_DIRECTORY) {
        strcpy(child_data[0].name, ".");
        child_data[0].inum = new_inum;
        strcpy(child_data[1].name, "..");
        child_data[1].inum = pinum;
        for (int i = 2; i < MAX_NUM_DIR_ENTRY_PER_BLOCK; i++) {
            child_data[i].inum = -1;
        }
    }

    // Create child's inode
    MFS_Stat_t child_stat;
    child_stat.type = type;
    child_stat.size = 0;
    if (type == MFS_DIRECTORY) {
        child_stat.size = MFS_BLOCK_SIZE;
    }
    struct INode child_inode;
    child_inode.stat = child_stat;
    for (int i = 0; i < MAX_FILE_BLOCKS; i++) {
        child_inode.ptr[i] = -1;
    }
    if (type == MFS_DIRECTORY) {
        child_inode.ptr[0] = ptr_child_data;
    }

    // Update parent
    MFS_DirEnt_t dir_data[MAX_NUM_DIR_ENTRY_PER_BLOCK];
    if (free_offset_found) {
        // Update parent's data
        lseek(fd, inode.ptr[free_idx_block], SEEK_SET);
        n_read = read(fd, &dir_data, sizeof(MFS_DirEnt_t) * MAX_NUM_DIR_ENTRY_PER_BLOCK);
        if (n_read < 0) {
            fprintf(stderr, "server:: creat cannot read parent block\n");
            exit(1);
        }
        strcpy(dir_data[free_idx_offset].name, name);
        dir_data[free_idx_offset].inum = new_inum;

        // Update parent's inode
        inode.ptr[free_idx_block] = ptr_parent_data;
    } else if (free_block_found) {
        // Add new parent's data
        strcpy(dir_data[0].name, name);
        dir_data[0].inum = new_inum;
        for (int i = 1; i < MAX_NUM_DIR_ENTRY_PER_BLOCK; i++) {
            dir_data[i].inum = -1;
        }
        
        // Update parent's inode
        inode.ptr[free_idx_block] = ptr_parent_data;
        if (inode.stat.size < (free_idx_block + 1) * MFS_BLOCK_SIZE) {
            inode.stat.size = (free_idx_block + 1) * MFS_BLOCK_SIZE;
        }
    }

    // Parent's inode map
    int parent_inode_map[MAX_INODES_MAP_ENTRIES];
    for (int i = 0; i < MAX_INODES_MAP_ENTRIES; i++) {
        parent_inode_map[i] = cached_map[parent_pieces_no][i];
    }

    // Update inode map & update cache
    parent_inode_map[parent_offset] = ptr_parent_inode;
    cached_map[parent_pieces_no][parent_offset] = ptr_parent_inode;

    // Child's inode map
    int child_inode_map[MAX_INODES_MAP_ENTRIES];
    for (int i = 0; i < MAX_INODES_MAP_ENTRIES; i++) {
        child_inode_map[i] = cached_map[pieces_no][i];
    }

    // Add child to inode map & cache
    child_inode_map[offset] = ptr_child_inode;
    cached_map[pieces_no][offset] = ptr_child_inode;

    // Update CR
    CR.endOfLog = ptr_end_of_log;
    if (parent_pieces_no == pieces_no) {
        child_inode_map[parent_offset] = ptr_parent_inode;
    } else {
        CR.iNodeMapPtr[parent_pieces_no] = ptr_parent_inode_map;
    }
    CR.iNodeMapPtr[pieces_no] = ptr_child_inode_map;

    // Write child_data
    if (type == MFS_DIRECTORY) {
        lseek(fd, ptr_child_data, SEEK_SET);
        int n_write = write(fd, &child_data, MFS_BLOCK_SIZE);
        if (n_write < 0) {
            fprintf(stderr, "server:: creat cannot write child directory block\n");
            exit(1);
        }
    }

    // Write child inode
    lseek(fd, ptr_child_inode, SEEK_SET);
    int n_write = write(fd, &child_inode, MFS_BLOCK_SIZE);
    if (n_write < 0) {
        fprintf(stderr, "server:: creat cannot write child inode\n");
        exit(1);
    }

    // Write parent data
    lseek(fd, ptr_parent_data, SEEK_SET);
    n_write = write(fd, &dir_data, MFS_BLOCK_SIZE);
    if (n_write < 0) {
        fprintf(stderr, "server:: creat cannot write parent directory block\n");
        exit(1);
    }

    // Write parent inode
    lseek(fd, ptr_parent_inode, SEEK_SET);
    n_write = write(fd, &inode, MFS_BLOCK_SIZE);
    if (n_write < 0) {
        fprintf(stderr, "server:: creat cannot write parent inode\n");
        exit(1);
    }

    // Write child inode map
    lseek(fd, ptr_child_inode_map, SEEK_SET);
    n_write = write(fd, &child_inode_map, MFS_BLOCK_SIZE);
    if (n_write < 0) {
        fprintf(stderr, "server:: creat cannot write child inode map\n");
        exit(1);
    }

    // Write parent inode map if it is NOT the same as child's inode map
    if (parent_pieces_no != pieces_no) {
        lseek(fd, ptr_parent_inode_map, SEEK_SET);
        n_write = write(fd, &parent_inode_map, MFS_BLOCK_SIZE);
        if (n_write < 0) {
            fprintf(stderr, "server:: creat cannot write parent inode map\n");
            exit(1);
        }
    }

    // Write CR
    lseek(fd, 0, SEEK_SET);
    n_write = write(fd, &CR, sizeof(struct CheckpointRegion));
    if (n_write < 0) {
        fprintf(stderr, "server:: creat cannot update CR\n");
        exit(1);
    }

    int fsync_status = fsync(fd);
    if (fsync_status < 0) {
        fprintf(stderr, "server:: creat cannot fsync to file\n");
        exit(1);
    }

    return 0;
}

int mfs_write(int inum, char *buffer, int block) {
    int disk_address = getAddressOrError(inum);
    if (disk_address == -1) {
        return -1;
    }

    if (block >= MAX_FILE_BLOCKS || block < 0) {
        return -1;
    }

    struct INode inode;
    lseek(fd, disk_address, SEEK_SET);
    int n_read = read(fd, &inode, sizeof(struct INode));
    if (n_read < 0) {
        fprintf(stderr, "server:: read cannot read inode\n");
        exit(1);
    }

    // Given file has to be a regular file
    if (inode.stat.type != MFS_REGULAR_FILE) {
        return -1;
    }

    // New address
    int ptr_data = CR.endOfLog;
    int ptr_inode = ptr_data + MFS_BLOCK_SIZE;
    int ptr_inode_map = ptr_inode + MFS_BLOCK_SIZE;
    int ptr_end_of_log = ptr_inode_map + MFS_BLOCK_SIZE;

    // Update INode
    inode.ptr[block] = ptr_data;
    if (inode.stat.size < (block + 1) * MFS_BLOCK_SIZE) {
        inode.stat.size = (block + 1) * MFS_BLOCK_SIZE;
    }

    // Update INode Map & cached map
    int pieces_no, offset;
    getPieceNoAndOffset(inum, &pieces_no, &offset);
    int inode_map[MAX_INODES_MAP_ENTRIES];
    for (int i = 0; i < MAX_INODES_MAP_ENTRIES; i++) {
        inode_map[i] = cached_map[pieces_no][i];
    }
    inode_map[offset] = ptr_inode;
    cached_map[pieces_no][offset] = ptr_inode;

    // Update CR
    CR.endOfLog = ptr_end_of_log;
    CR.iNodeMapPtr[pieces_no] = ptr_inode_map;

    // Write Data
    lseek(fd, ptr_data, SEEK_SET);
    int n_write = write(fd, buffer, MFS_BLOCK_SIZE);
    if (n_write < 0) {
        fprintf(stderr, "server:: write cannot write child block\n");
        exit(1);
    }

    // Write INode
    lseek(fd, ptr_inode, SEEK_SET);
    n_write = write(fd, &inode, MFS_BLOCK_SIZE);
    if (n_write < 0) {
        fprintf(stderr, "server:: write cannot write inode\n");
        exit(1);
    }

    // Write INode Map
    lseek(fd, ptr_inode_map, SEEK_SET);
    n_write = write(fd, &inode_map, MFS_BLOCK_SIZE);
    if (n_write < 0) {
        fprintf(stderr, "server:: write cannot write inode map\n");
        exit(1);
    }

    // Write CR
    lseek(fd, 0, SEEK_SET);
    n_write = write(fd, &CR, sizeof(struct CheckpointRegion));
    if (n_write < 0) {
        fprintf(stderr, "server:: write cannot update CR\n");
        exit(1);
    }

    int fsync_status = fsync(fd);
    if (fsync_status < 0) {
        fprintf(stderr, "server:: write cannot fsync to file\n");
        exit(1);
    }

    return 0;
}

int mfs_unlink(int pinum, char *name) {
    int disk_address = getAddressOrError(pinum);
    if (disk_address == -1) {
        return -1;
    }

    int parent_pieces_no, parent_offset;
    getPieceNoAndOffset(pinum, &parent_pieces_no, &parent_offset);

    struct INode parent_inode;
    lseek(fd, disk_address, SEEK_SET);
    int n_read = read(fd, &parent_inode, sizeof(struct INode));
    if (n_read < 0) {
        fprintf(stderr, "server:: lookup cannot read parent inode\n");
        exit(1);
    }

    if (parent_inode.stat.type != MFS_DIRECTORY) {
        return -1;
    }

    for (int i = 0; i < MAX_FILE_BLOCKS; i++) {
        if (parent_inode.ptr[i] != -1) {
            MFS_DirEnt_t directory_entry[MAX_NUM_DIR_ENTRY_PER_BLOCK];
            lseek(fd, parent_inode.ptr[i], SEEK_SET);
            n_read = read(fd, &directory_entry, sizeof(MFS_DirEnt_t) * MAX_NUM_DIR_ENTRY_PER_BLOCK);
            if (n_read < 0) {
                fprintf(stderr, "server:: unlink cannot read parent block\n");
                exit(1);
            }

            for (int j = 0; j < MAX_NUM_DIR_ENTRY_PER_BLOCK; j++) {
                // Found
                if (directory_entry[j].inum != -1 && strcmp(directory_entry[j].name, name) == 0) {
                    // Child offset
                    int child_pieces_no, child_offset;
                    getPieceNoAndOffset(directory_entry[j].inum, &child_pieces_no, &child_offset);
                    
                    // Read child inode
                    struct INode child_inode;
                    lseek(fd, cached_map[child_pieces_no][child_offset], SEEK_SET);
                    n_read = read(fd, &child_inode, sizeof(struct INode));
                    if (n_read < 0) {
                        fprintf(stderr, "server:: unlink cannot read child inode\n");
                        exit(1);
                    }

                    // Check if dir
                    if (child_inode.stat.type == MFS_DIRECTORY) {
                        // Check if not empty
                        for (int k = 0; k < MAX_FILE_BLOCKS; k++) {
                            if (child_inode.ptr[k] != -1) {
                                MFS_DirEnt_t child_dir_entry[MAX_NUM_DIR_ENTRY_PER_BLOCK];
                                lseek(fd, child_inode.ptr[k], SEEK_SET);
                                n_read = read(fd, &child_dir_entry, sizeof(MFS_DirEnt_t) * MAX_NUM_DIR_ENTRY_PER_BLOCK);
                                if (n_read < 0) {
                                    fprintf(stderr, "server:: lookup cannot read child block\n");
                                    exit(1);
                                }
                                
                                for (int m = 0; m < MAX_NUM_DIR_ENTRY_PER_BLOCK; m++) {
                                    if (child_dir_entry[m].inum != -1 && child_dir_entry[m].inum != directory_entry[j].inum && child_dir_entry[m].inum != pinum) {
                                        return -1;
                                    }
                                }
                            }
                        }
                    }

                    // New address
                    int ptr_parent_data = CR.endOfLog;
                    int ptr_parent_inode = ptr_parent_data + MFS_BLOCK_SIZE;
                    int ptr_child_inode_map = ptr_parent_inode + MFS_BLOCK_SIZE;
                    int ptr_parent_inode_map = ptr_child_inode_map + MFS_BLOCK_SIZE;
                    int ptr_end_of_log = ptr_parent_inode_map;
                    if (parent_pieces_no != child_pieces_no) {
                        ptr_end_of_log += MFS_BLOCK_SIZE;
                    }

                    // Update parent data
                    directory_entry[j].inum = -1;

                    // Update parent inode
                    parent_inode.ptr[i] = ptr_parent_data;

                    // Parent's inode map
                    int parent_inode_map[MAX_INODES_MAP_ENTRIES];
                    for (int i = 0; i < MAX_INODES_MAP_ENTRIES; i++) {
                        parent_inode_map[i] = cached_map[parent_pieces_no][i];
                    }

                    // Update inode map & update cache
                    parent_inode_map[parent_offset] = ptr_parent_inode;
                    cached_map[parent_pieces_no][parent_offset] = ptr_parent_inode;

                    // Child's inode map
                    int child_inode_map[MAX_INODES_MAP_ENTRIES];
                    for (int i = 0; i < MAX_INODES_MAP_ENTRIES; i++) {
                        child_inode_map[i] = cached_map[child_pieces_no][i];
                    }

                    // Remove child from inode map & cache
                    child_inode_map[child_offset] = -1;
                    cached_map[child_pieces_no][child_offset] = -1;

                    // Update CR
                    CR.endOfLog = ptr_end_of_log;
                    if (parent_pieces_no == child_pieces_no) {
                        child_inode_map[parent_offset] = ptr_parent_inode;
                    } else {
                        CR.iNodeMapPtr[parent_pieces_no] = ptr_parent_inode_map;
                    }
                    CR.iNodeMapPtr[child_pieces_no] = ptr_child_inode_map;

                    // Write parent data
                    lseek(fd, ptr_parent_data, SEEK_SET);
                    int n_write = write(fd, &directory_entry, MFS_BLOCK_SIZE);
                    if (n_write < 0) {
                        fprintf(stderr, "server:: unlink cannot write parent directory block\n");
                        exit(1);
                    }

                    // Write parent inode
                    lseek(fd, ptr_parent_inode, SEEK_SET);
                    n_write = write(fd, &parent_inode, MFS_BLOCK_SIZE);
                    if (n_write < 0) {
                        fprintf(stderr, "server:: unlink cannot write parent inode\n");
                        exit(1);
                    }
    
                    // Write child inode map
                    lseek(fd, ptr_child_inode_map, SEEK_SET);
                    n_write = write(fd, &child_inode_map, MFS_BLOCK_SIZE);
                    if (n_write < 0) {
                        fprintf(stderr, "server:: unlink cannot write child inode map\n");
                        exit(1);
                    }

                    // Write parent inode map if it is NOT the same as child's inode map
                    if (parent_pieces_no != child_pieces_no) {
                        lseek(fd, ptr_parent_inode_map, SEEK_SET);
                        n_write = write(fd, &parent_inode_map, MFS_BLOCK_SIZE);
                        if (n_write < 0) {
                            fprintf(stderr, "server:: unlink cannot write parent inode map\n");
                            exit(1);
                        }
                    }

                    // Write CR
                    lseek(fd, 0, SEEK_SET);
                    n_write = write(fd, &CR, sizeof(struct CheckpointRegion));
                    if (n_write < 0) {
                        fprintf(stderr, "server:: unlink cannot update CR\n");
                        exit(1);
                    }

                    int fsync_status = fsync(fd);
                    if (fsync_status < 0) {
                        fprintf(stderr, "server:: unlink cannot fsync to file\n");
                        exit(1);
                    }

                    return 0;
                }
            }
        }
    }

    return 0;
}

void setup() {
    // Setup connection port
    sd = UDP_Open(portnum);
    if (sd < 0) {
        fprintf(stderr, "server:: cannot open port %d\n", portnum);
        exit(1);
    }

    // Setup cache
    for (int i = 0; i < MAX_NUM_INODES_PIECES; i++) {
        for (int j = 0; j < MAX_INODES_MAP_ENTRIES; j++) {
            cached_map[i][j] = -1;
        }
    }

    // Read or create file image
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
            // root_stat.size = 2 * DIR_ENTRY_SIZE;
            root_stat.size = MFS_BLOCK_SIZE;
            struct INode root_inode;
            root_inode.stat = root_stat;
            root_inode.ptr[0] = sizeof(struct CheckpointRegion);
            for (int i = 1; i < MAX_FILE_BLOCKS; i++) {
                root_inode.ptr[i] = -1;
            }

            // Initialize inode map
            int first_inode_map[MAX_INODES_MAP_ENTRIES];
            first_inode_map[0] = sizeof(struct CheckpointRegion) + MFS_BLOCK_SIZE;
            cached_map[0][0] = sizeof(struct CheckpointRegion) + MFS_BLOCK_SIZE;
            for (int i = 1; i < MAX_INODES_MAP_ENTRIES; i++) {
                first_inode_map[i] = -1;
                cached_map[0][i] = -1;
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
            n_read = read(fd, &currINodeMap, sizeof(int) * MAX_INODES_MAP_ENTRIES);
            if (n_read < 0) {
                fprintf(stderr, "server:: cannot read inode map from the existing file\n");
                exit(1);
            }

            for (int j = 0; j < MAX_INODES_MAP_ENTRIES; j++) {
                cached_map[i][j] = currINodeMap[j];
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

    while (1) {
        struct sockaddr_in addr;
        char message[BUFFER_SIZE];
        int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
        if (rc > 0) {
            Method method;
            memcpy(&method, message, sizeof(Method));

            if (method == Lookup) {
                MFS_Lookup_Request request;
                memcpy(&request, message, sizeof(MFS_Lookup_Request));

                int status = mfs_lookup(request.pinum, request.name);

                char response_buffer[BUFFER_SIZE];
                memcpy(response_buffer, &status, sizeof(int));
                rc = UDP_Write(sd, &addr, response_buffer, BUFFER_SIZE);
            } else if (method == Stat) {
                MFS_Stat_Request request;
                MFS_Stat_t result;
                memcpy(&request, message, sizeof(MFS_Stat_Request));

                MFS_Stat_Response response;
                response.status = mfs_stat(request.inum, &result);
                response.stat = result;

                char response_buffer[BUFFER_SIZE];
                memcpy(response_buffer, &response, sizeof(MFS_Stat_Response));
                rc = UDP_Write(sd, &addr, response_buffer, BUFFER_SIZE);
            } else if (method == Write) {
                MFS_Write_Request request;
                memcpy(&request, message, sizeof(MFS_Write_Request));

                int status = mfs_write(request.inum, request.buffer, request.block);

                char response_buffer[BUFFER_SIZE];
                memcpy(response_buffer, &status, sizeof(int));
                rc = UDP_Write(sd, &addr, response_buffer, BUFFER_SIZE);
            } else if (method == Read) {
                MFS_Read_Request request;
                memcpy(&request, message, sizeof(MFS_Read_Request));

                MFS_Read_Response response;
                response.status = mfs_read(request.inum, response.buffer, request.block);

                char response_buffer[BUFFER_SIZE];
                memcpy(response_buffer, &response, sizeof(MFS_Read_Response));
                rc = UDP_Write(sd, &addr, response_buffer, BUFFER_SIZE);
            } else if (method == Creat) {
                MFS_Creat_Request request;
                memcpy(&request, message, sizeof(MFS_Creat_Request));

                int status = mfs_creat(request.pinum, request.type, request.name);

                char response_buffer[BUFFER_SIZE];
                memcpy(response_buffer, &status, sizeof(int));
                rc = UDP_Write(sd, &addr, response_buffer, BUFFER_SIZE); 
            } else if (method == Unlink) {
                MFS_Unlink_Request request;
                memcpy(&request, message, sizeof(MFS_Unlink_Request));

                int status = mfs_unlink(request.pinum, request.name);

                char response_buffer[BUFFER_SIZE];
                memcpy(response_buffer, &status, sizeof(int));
                rc = UDP_Write(sd, &addr, response_buffer, BUFFER_SIZE); 
            } else if (method == ShutDown) {
                int status = 0;

                char response_buffer[BUFFER_SIZE];
                memcpy(response_buffer, &status, sizeof(int));
                rc = UDP_Write(sd, &addr, response_buffer, BUFFER_SIZE);

                shutDown(); 
            } else {
                int status = -1;
                printf("server:: invalid response received\n");
                char response_buffer[BUFFER_SIZE];
                memcpy(response_buffer, &status, sizeof(int));
                rc = UDP_Write(sd, &addr, response_buffer, BUFFER_SIZE);
            }
        } 
    }
    
    return 0; 
}
