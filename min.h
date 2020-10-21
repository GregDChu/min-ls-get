#include <stdint.h>

#ifndef MIN_H
#define MIN_H

#define VERBOSE 0x01
#define PART 0x02
#define SUBPART 0x04
#define EMPTY 0x00
#define SET(FLAGS, BIT) (FLAGS |= BIT)
#define CLEAR(FLAGS, BIT) (FLAGS &= (~BIT))
#define GET(FLAGS, BIT) (FLAGS & BIT)

#define DIRECT_ZONES 7
#define MAX_REDIRECTS 3
#define DIRECT 0
#define INDIRECT 1
#define TWO_INDIRECT 2
#define UNUSED 3

#define SECTOR_SIZE 512
#define PART_OFFSET 0x1BE
#define SB_OFFSET 1024
#define NODEMAP_BLOCK_OFFSET 2

#define NULL_CHAR_SIZE 1
#define MAGIC_NUM_MINIX 0x4D5A
#define PART_TYPE_MINIX 0x81
#define PART_VERIF_BYTES 510
#define PVERIF_BYTE_COUNT 2
#define PART_TABLE_BYTE_A 0x55
#define PART_TABLE_BYTE_B 0xAA

#define SUCCESS 0
#define FAIL 1

#define BAD_ALLOC_ERROR(F) "%s: Cannot allocate space for struct\n", F
#define BAD_READ_ERROR(F) "%s: Cannot read from file provided\n", F
#define NO_IMG_DATA_ERROR(F) "%s: No image metadata is loaded\n", F
#define BAD_FS_ERROR "Bad magic number. (0x%04x)\n\
This doesn't look like a MINIX filesystem.\n"
#define PRINT_BUFF_SIZE 40

#define ROOT_INODE 1
#define DIR_MASK 0040000
#define PERMS "drwxrwxrwx\0"
#define PERM_COUNT 10
#define FILE_TYPE 0170000
#define REG_FILE 0100000
#define DIR_TYPE 0040000
#define O_RD_PERM 0000400
#define O_WR_PERM 0000200
#define O_EX_PERM 0000100
#define G_RD_PERM 0000040
#define G_WR_PERM 0000020
#define G_EX_PERM 0000010
#define OTH_RD_PERM 0000004
#define OTH_WR_PERM 0000002
#define OTH_EX_PERM 0000001

typedef uint8_t byte;
typedef struct part_entry *part_table;
typedef struct part_entry
{
    uint8_t bootind;
    uint8_t start_head;
    uint8_t start_sec;
    uint8_t start_cyl;
    uint8_t type;
    uint8_t last_head;
    uint8_t last_sec;
    uint8_t last_cyl;
    uint32_t lowsec;
    uint32_t size;
} *part_table;

typedef struct super_block *super_block;
typedef struct super_block
{
    uint32_t ninodes;
    uint16_t pad1;
    int16_t i_blocks;
    int16_t z_blocks;
    uint16_t firstdata;
    int16_t log_zone_size;
    int16_t pad2;
    uint32_t max_file;
    uint32_t zones;
    int16_t magic;
    int16_t pad3;
    uint16_t blocksize;
    uint8_t subversion;

} *super_block;

typedef struct inode *inode;
typedef struct inode
{
    uint16_t mode;
    uint16_t links;
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    int32_t atime;
    int32_t mtime;
    int32_t ctime;
    uint32_t zone[DIRECT_ZONES];
    uint32_t indirect;
    uint32_t two_indirect;
    uint32_t unused;
} *inode;

typedef struct dirent *dirent;
typedef struct dirent 
{
    uint32_t inode;
    unsigned char name[60];
} *dirent;

typedef struct metadata *metadata;
typedef struct metadata
{
    FILE *img;
    super_block sb;
    uint32_t part_offset;
    uint32_t inode_offset;
    uint32_t zone_offset;
    uint32_t zone_size;
} *metadata;

int load_image(char *file, byte flags, byte p, byte s);
int load_partition(FILE *img, part_table part, uint32_t offset, byte p);
int load_superblock(FILE *img, super_block sb, uint32_t offset);
void close_image();

int path_to_inode(char *path, inode dest);
int read_inode(uint32_t num, inode dest);
int read_dirents(inode file, dirent dest);
int read_file(inode file, void *dest);
int write_contents(inode file, FILE *where);
uint32_t read_indirect(uint32_t zone, byte redirects, 
                       void *dest, uint32_t bytes);
uint32_t read_zones(uint32_t *zones, uint32_t size, 
                    void *dest, uint32_t bytes);
uint32_t *get_zone_group(inode node, byte redirects);

void print_file(inode file, char *name);
void format_perms(uint16_t mode, char *where);
void print_dirents(inode file, char *filepath);

void print_inode(inode node);
void print_sb();
#endif