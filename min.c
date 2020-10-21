#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "min.h"

uint32_t perm_mask[PERM_COUNT] = { 
    DIR_TYPE, O_RD_PERM, O_WR_PERM, O_EX_PERM,
              G_RD_PERM, G_WR_PERM, G_EX_PERM,
              OTH_RD_PERM, OTH_WR_PERM, OTH_EX_PERM
    };

metadata mdata = NULL;

int load_image(char *file, byte flags, byte p, byte s)
{
    // Allocate space for all of meta data about image
    FILE *img;
    struct part_entry part;
    super_block sb;
    uint32_t part_offset;
    uint32_t offset;
    // Open the image
    if((img = fopen(file, "r")) == NULL)
    {
        perror("load_image");
        return FAIL;
    }
    // Check if user specified a partition
    part_offset = 0;
    if(GET(flags, PART))
    {
        // Load parition
       if(load_partition(img, &part, part_offset, p) == FAIL)
        {
            fclose(img);
            return FAIL;
        }
        part_offset = part.lowsec * SECTOR_SIZE;
    }
    
    // Check if user specified a subpartition
    if(GET(flags, SUBPART))
    {
        // Load subpartition
        if(load_partition(img, &part, part_offset, s) == FAIL)
        {
            fclose(img);
            return FAIL;
        }
        part_offset = part.lowsec * SECTOR_SIZE;
    }
    // Allocate space for super block
    if((sb = calloc(1, sizeof(struct super_block))) == NULL)
    {
        fprintf(stderr, BAD_ALLOC_ERROR("load_image"));
        fclose(img);
        return FAIL;
    }
    // Load super block
    if(load_superblock(img, sb, part_offset) == FAIL)
    {
        fclose(img);
        free(sb);
        return FAIL;
    }
    // Allocate space for meta data
    mdata = calloc(1, sizeof(struct metadata));
    if(mdata == NULL)
    {
        fprintf(stderr, BAD_ALLOC_ERROR("mdata"));
        fclose(img);
        free(sb);
        return FAIL;
    }
    // Store all data
    mdata->img = img;
    mdata->sb = sb;
    mdata->part_offset = part_offset;
    // Store the inode offset
    offset = mdata->part_offset + (NODEMAP_BLOCK_OFFSET * sb->blocksize);
    offset += (sb->i_blocks * sb->blocksize);
    offset += (sb->z_blocks * sb->blocksize);
    mdata->inode_offset = offset;
    // Store the zone size
    mdata->zone_size = (sb->blocksize << sb->log_zone_size);
    // Store the zone offset
    offset += sb->firstdata * mdata->zone_size;
    mdata->zone_offset = offset;
    return SUCCESS;
}

void close_image()
{
    if(mdata != NULL)
    {
        fclose(mdata->img);
        free(mdata->sb);
        free(mdata);
        mdata = NULL;
    }
}

int load_partition(FILE *img, part_table part, uint32_t offset, byte p)
{
    byte buff[PVERIF_BYTE_COUNT];
    uint32_t table_offset;
    // Verify partition
    if(fseek(img, PART_VERIF_BYTES + offset, SEEK_SET) == -1)
    {
        perror("load_partition");
        return FAIL;
    }
    if(fread(&buff, sizeof(byte), PVERIF_BYTE_COUNT, img) < PVERIF_BYTE_COUNT)
    {
        fprintf(stderr, BAD_READ_ERROR("load_partition"));
        return FAIL;
    }
    if(buff[0] != PART_TABLE_BYTE_A || buff[1] != PART_TABLE_BYTE_B)
    {
        fprintf(stderr, "No partition table found in image\n");
        return FAIL;
    }
    // Load the partiton
    table_offset = (p * sizeof(struct part_entry));
    if(fseek(img, offset + PART_OFFSET + table_offset , SEEK_SET) == -1)
    {
        perror("load_partition");
        return FAIL;
    }
    if(fread(part, sizeof(struct part_entry), 1, img) < 1)
    {
        fprintf(stderr, BAD_READ_ERROR("load_image"));
        return FAIL;
    }
    // Verify the partition
    if(part->type != PART_TYPE_MINIX)
    {
        fprintf(stderr, "Invalid partition table.\n");
        return FAIL;
    }
    return SUCCESS;
}

int load_superblock(FILE *img, super_block sb, uint32_t offset)
{
    // Load the super block
    if(fseek(img, offset + SB_OFFSET, SEEK_SET) == -1)
    {
        perror("get_sblock()");
        return FAIL;
    }
    if(fread(sb, sizeof(struct super_block), 1, img) < 1)
    {
        fprintf(stderr, BAD_READ_ERROR("load_image"));
        return FAIL;
    }
    // Verify the super block
    if(sb->magic != MAGIC_NUM_MINIX)
    {
        fprintf(stderr, BAD_FS_ERROR, sb->magic);
        return FAIL;
    }
    return SUCCESS;
}

int read_inode(uint32_t num, inode dest)
{
    // Verify that image was loaded
    if(mdata == NULL)
    {
        fprintf(stderr, NO_IMG_DATA_ERROR("read_zones"));
        return FAIL;
    }
    // Verify that input is useable
    size_t offset;
    if(num == 0 || dest == NULL)
    {
        fprintf(stderr, "read_inode: Bad input.\n");
        return FAIL;
    }
    // Calculate location of inode via offset
    offset = mdata->inode_offset;
    // Set location of stream
    offset += (num - 1) * sizeof(struct inode);
    if(fseek(mdata->img, offset, SEEK_SET) == -1)
    {
        perror("get_inode()");
        return FAIL;
    }
    // Write into destination inode
    if(fread(dest, sizeof(struct inode), 1, mdata->img) < 1)
    {
        fprintf(stderr, "read_inode: Cannot inode %d\n", num);
        return FAIL;
    }
    return SUCCESS;
}

int read_dirents(inode file, dirent dest)
{
    // Gather dirents and write to dest
    if(read_file(file, (void *)dest) == FAIL)
    {
        return FAIL;
    }
    return SUCCESS;
}

int read_file(inode file, void *dest)
{
    uint32_t bytes_read = 0;
    uint32_t bytes_left = file->size;
    void *dest_ptr = dest;
    byte redirects = 0;
    uint32_t *zone_nums;
    // Verify pointers before using them
    if(file == NULL || dest == NULL)
    {
        fprintf(stderr, "read_file: Bad input\n");
        return FAIL;
    }
    // Keep reading until no more bytes left
    // to read or run out of redirects
    while(bytes_left > 0 && redirects <= MAX_REDIRECTS)
    {
        // Get the zone group
        zone_nums = get_zone_group(file, redirects);
        if(zone_nums == NULL)
        {
            fprintf(stderr, "read_file: Bad zone group.\n");
            return FAIL;
        }
        // Read directs
        if(redirects == DIRECT)
        {
            bytes_read = read_zones(zone_nums, DIRECT_ZONES, 
                                    dest_ptr, file->size);
        }
        // Read indirects
        else
        {
            bytes_read = read_indirect(*zone_nums,
                         redirects, dest_ptr, bytes_left);
        }
        // Check if read failed
        if(bytes_left != 0 && bytes_read == 0)
        {
            fprintf(stderr, "read_file: Failed to read file\n");
            return FAIL;
        }
        // Update bytes read
        bytes_left -= bytes_read;
        // Update write destination
        dest_ptr += bytes_read;
        // Update redirects required for next read
        redirects++;
    }
    // Check if some bytes were missed
    if(bytes_left > 0)
    {
        return FAIL;  
    }
    return SUCCESS;
}

uint32_t read_indirect(uint32_t zone, byte redirects, 
void *dest, uint32_t bytes)
{
    uint32_t *zone_nums;
    void *dest_ptr = dest;
    uint32_t bytes_left = bytes;
    uint32_t bytes_read;
    int zindex;
    // Verify that pointers are useable
    if(mdata == NULL)
    {
        fprintf(stderr, NO_IMG_DATA_ERROR("read_zones"));
        return 0;
    }
    else if(dest == NULL)
    {
        fprintf(stderr, "read_zones: Bad input.\n");
        return 0;
    }
    // If zone data is raw data, read 
    // zone data directly into destination
    if(redirects == DIRECT)
    {
        bytes_read = read_zones(&zone, 1, dest, bytes_left);
        // Check for errors during during reading
        if(bytes_read == 0 && bytes_left > 0)
        {
            return 0;
        }
        return bytes_read;
    }
    // Otherwise, read zone numbers and 
    // perform another level of indirect reading

    // Allocate space for zone numbers
    zone_nums = calloc(sizeof(byte), mdata->zone_size);
    if(zone_nums == NULL)
    {
        fprintf(stderr, BAD_ALLOC_ERROR("read_indirect"));
        return 0;
    }
    // Check for a hole
    if(zone == 0)
    {
        // Set all zone numbers to zero
        memset(zone_nums, 0, mdata->zone_size);
    }
    // Otherwise copy a singular zone's data locally
    else
    {
        bytes_read = read_zones(&zone, 1, zone_nums, mdata->zone_size);
        if(bytes_read == 0 && bytes_left > 0)
        {
            return 0;
        }
    }
    // Read all of the zone group's sub data into the destination
    // until the zone has been read completely or all file bytes
    // have been read
    zindex = 0;
    while(bytes_left > 0 && 
          zindex < (mdata->zone_size / sizeof(zone_nums[0])))
    {
        // Read indirect group
        bytes_read = read_indirect(zone_nums[zindex], 
                     redirects - 1, dest_ptr, bytes_left);
        // Update bytes left
        bytes_left -= bytes_read;
        // Update destination pointer
        dest_ptr += bytes_read;
        // Update zone index
        zindex++;
    }
    // Free local data
    free(zone_nums);
    // Return number of bytes read
    return bytes - bytes_left;
}

uint32_t read_zones(uint32_t *zones, uint32_t size, 
void *dest, uint32_t bytes)
{
    int zindex = 0;
    void *dest_ptr = dest;
    uint32_t bytes_read = 0;
    uint32_t bytes_to_read;
    uint32_t offset;
    // Check that all pointers are initialized
    if(mdata == NULL)
    {
        fprintf(stderr, NO_IMG_DATA_ERROR("read_zones"));
        return 0;
    }
    else if(zones == NULL || dest == NULL)
    {
        fprintf(stderr, "read_zones: Bad input.\n");
        return 0;
    }
    // Continue reading zones until out of zones or
    // bytes requested has been fulfilled
    while(zindex < size && bytes_read < bytes)
    {
        // Check for a hole
        if(zones[zindex] == 0)
        {
            memset(dest_ptr, 0, mdata->zone_size);
            bytes_to_read = mdata->zone_size;
        }
        // Otherwise, read raw data
        else
        {
            // Calculate the zone location via offset
            // from the start of the disk
            offset = mdata->part_offset + (zones[zindex] * mdata->zone_size);
            // Locate a zone
            if(fseek(mdata->img, offset, SEEK_SET) == -1)
            {
                perror("read_zones");
                return 0;
            }
            // Calculate number of bytes to read
            bytes_to_read = mdata->zone_size;
            if((bytes - bytes_read) < mdata->zone_size)
            {
                bytes_to_read = bytes - bytes_read;
            }
            // Read the zone
            if(fread(dest_ptr, sizeof(byte), bytes_to_read, mdata->img) <
            bytes_to_read)
            {
                fprintf(stderr, BAD_READ_ERROR("read_zones"));
                return 0;
            }
        }
        // Update bytes read
        bytes_read += bytes_to_read;
        // Update write destination
        dest_ptr += mdata->zone_size;
        // Update zone index
        zindex++;
    }
    return bytes_read;
}

uint32_t *get_zone_group(inode node, byte redirects)
{
    // Used to get the zone group for
    // indirect zone reading
    switch(redirects)
    {
        case DIRECT:
            return &(node->zone[0]);
        case INDIRECT:
            return &(node->indirect);
        case TWO_INDIRECT:
            return &(node->two_indirect);
        case UNUSED:
            return &(node->unused);
        default:
            return NULL;
    }
}


int write_contents(inode file, FILE *where)
{
    void *buff;
    // Allocate space for a buffer to temporarily
    // write to
    buff = calloc(file->size, sizeof(byte));
    if(buff == NULL)
    {
        fprintf(stderr, "write_contents: Cannot alloc mem for buffer.\n");
        return FAIL;
    }
    // Read the file contents, check for errors
    if(read_file(file, buff) == FAIL)
    {
        free(buff);
        return FAIL;
    }
    // Write the locally read data into the desired location
    if(fwrite(buff, sizeof(byte), file->size, where) < file->size)
    {
        fprintf(stderr, "write_contents: Failed to write entire file.\n");
        free(buff);
        return FAIL;
    }
    // Bookkeeping
    free(buff);
    return SUCCESS;
}


int path_to_inode(char *path, inode dest)
{
    char *local_path;
    char *file_name;
    struct inode node;
    dirent entries;
    int i;
    // Check that pointers are usable
    if(dest == NULL)
    {
        fprintf(stderr, "get_file: Bad input.\n");
        return FAIL;
    }
    // Get root inode if path is empty
    if(path == NULL)
    {
        if(read_inode(ROOT_INODE, dest) == FAIL)
        {
            fprintf(stderr, "get_file: Can't read root inode.\n");
            return FAIL;
        }
        return SUCCESS;
    }
    // Allocate space for local path
    local_path = calloc(strlen(path) + NULL_CHAR_SIZE, sizeof(char));
    if(local_path == NULL)
    {
        fprintf(stderr, BAD_ALLOC_ERROR("path_to_inode"));
        return FAIL;
    }
    // Store path locally for strtok to mess around with
    memcpy(local_path, path, strlen(path));
    local_path[strlen(path)] = '\0';
    // Read root directory
    if(read_inode(ROOT_INODE, &node) == FAIL)
    {
        free(local_path);
        return FAIL;
    }
    // Read root directory entries
    entries = calloc(node.size, sizeof(byte));
    if(entries == NULL)
    {
        fprintf(stderr, BAD_ALLOC_ERROR("get_file"));
        free(local_path);
        return FAIL;
    }
    if(read_dirents(&node, entries) == FAIL)
    {
        free(entries);
        free(local_path);
        return FAIL;
    }
    // Tokenize local path
    file_name = strtok(local_path, "/");
    i = 0;
    // Keep reading next file until
    // end of path reached
    while(file_name != NULL)
    {
        // Locate the next desired file
        while(i < (node.size / sizeof(struct dirent)) &&
              strcmp(file_name, (char*)entries[i].name) != 0)
        {
            i++;
        }
        // If it's not found, throw an error and return
        if(i == (node.size / sizeof(struct dirent)))
        {
            fprintf(stderr, "%s: File not found.\n", file_name);
            free(entries);
            free(local_path);
            return FAIL;
        }
        // Otherwise, update inode
        if(read_inode(entries[i].inode, &node) == FAIL)
        {
            free(local_path);
            return FAIL;
        }
        // Reset entry index
        i = 0;
        // Update to next file name
        file_name = strtok(NULL, "/");
        // Check if there is more directories to traverse
        // but the current file isn't a directory
        if(file_name != NULL && (node.mode & FILE_TYPE) != DIR_TYPE)
        {
            // Throw an error
            fprintf(stderr, "%s: Not a directory\n", file_name);
            return FAIL;
        }
        // Otherwise, free memory holding previous directory's
        // entries and get new directory entries
        if(entries != NULL)
        {
            free(entries);
        }
        entries = calloc(node.size, sizeof(byte));
        if(entries == NULL)
        {
            fprintf(stderr, BAD_ALLOC_ERROR("get_file"));
            free(local_path);
            return FAIL;
        }
        if(read_dirents(&node, entries) == FAIL)
        {
            free(entries);
            free(local_path);
            return FAIL;
        }
    }
    // Bookkeeping: a final free of allocated mem
    free(entries);
    free(local_path);
    // Write it to destination
    memcpy(dest, &node, sizeof(node));
    return SUCCESS;
}

void print_file(inode file, char *name)
{
    // Print data about the file
    char perms[sizeof(PERMS)];
    // Get permission data
    format_perms(file->mode, perms);
    // Print!
    fprintf(stdout, "%s %9d %s\n", 
            perms, file->size, 
            name);
}

void format_perms(uint16_t mode, char *dest)
{
    // Copy permissions string into destination
    memcpy(dest, PERMS, sizeof(PERMS));
    // Iterate through each permission
    int perm_index = 0;
    while(perm_index < PERM_COUNT)
    {
        // If the permission is denied
        if(!(mode & perm_mask[perm_index]))
        {
            // Update permission value in dest string
            dest[perm_index] = '-';
        }
        perm_index++;
    }
}

void print_dirents(inode file, char *filepath)
{
    dirent entries;
    struct inode node;
    int i;
    // Check that pointer is usable
    if(file == NULL)
    {
        fprintf(stderr, "print_dirents: Bad input.\n");
        return;
    }
    // Allocate space for entries
    entries = calloc(file->size, sizeof(byte));
    if(entries == NULL)
    {
        fprintf(stderr, BAD_ALLOC_ERROR("print_dirents"));
        return;
    }
    // Read directory entries
    if(read_dirents(file, entries) == FAIL)
    {
        fprintf(stderr, "print_dirents: Failed to read entries.\n");
        return;
    }
    // Print directory first
    fprintf(stdout, "%s:\n", filepath == NULL ? "/" : filepath);
    // Print each file info from entry array
    i = 0;
    for(; i < file->size / sizeof(struct dirent); i++)
    {
        // Check that the inode number isn't 0 and
        // that the inode can be located and read properly
        if(entries[i].inode == 0 ||
           read_inode(entries[i].inode, &node) == FAIL)
        {
            // Skip the entry
            continue;
        }
        // Print the entry info
        print_file(&node, (char *)entries[i].name);
    }
    // Free memory for entries
    free(entries);
}

void print_sb()
{
    if(mdata != NULL)
    {
        fprintf(stdout, "\nSuperblock Contents:\nStored Fields:\n");
        fprintf(stdout, "  %-15s%u\n", "ninodes", mdata->sb->ninodes);
        fprintf(stdout, "  %-15s%u\n", "i_blocks", mdata->sb->i_blocks);
        fprintf(stdout, "  %-15s%u\n", "z_blocks", mdata->sb->z_blocks);
        fprintf(stdout, "  %-15s%u\n", "firstdata", mdata->sb->firstdata);
        fprintf(stdout, "  %-15s%u (zonesize: %u)\n", "log_zone_size",
                mdata->sb->log_zone_size, mdata->zone_size);
        fprintf(stdout, "  %-15s%u\n", "max_file", mdata->sb->max_file);
        fprintf(stdout, "  %-15s0x%4x\n", "magic", mdata->sb->magic);
        fprintf(stdout, "  %-15s%u\n", "zones", mdata->sb->zones);
        fprintf(stdout, "  %-15s%u\n", "blocksize", mdata->sb->blocksize);
        fprintf(stdout, "  %-15s%u\n", "subversion", mdata->sb->subversion);
    }
}

void print_inode(inode node)
{
    char perms[sizeof(PERMS)];
    char buffer[PRINT_BUFF_SIZE];
    struct tm* tm_info;
    time_t tmp_time;
    format_perms(node->mode, perms);
    fprintf(stdout, "\nFile inode:\n");
    fprintf(stdout, "  %-20s0x%x (%s)\n", "uint16_t mode", node->mode, perms);
    fprintf(stdout, "  %-20s%u\n", "uint16_t links", node->links);
    fprintf(stdout, "  %-20s%u\n", "uint16_t uid", node->uid);
    fprintf(stdout, "  %-20s%u\n", "uint16_t gid", node->gid);
    fprintf(stdout, "  %-20s%u\n", "uint32_t size", node->size);
    tmp_time = (time_t)node->atime;
    tm_info = localtime(&tmp_time);
    if(strftime(buffer, PRINT_BUFF_SIZE, "%a %b %d %H:%M:%S %Y", tm_info) == 0)
    {
        fprintf(stderr, "print_inode: Couldn't format time\n");
    }
    fprintf(stdout, "  %-20s%u | %s\n", "uint32_t atime", node->atime, buffer);
    tmp_time = (time_t)node->mtime;
    tm_info = localtime(&tmp_time);
    if(strftime(buffer, PRINT_BUFF_SIZE, "%a %b %d %H:%M:%S %Y", tm_info) == 0)
    {
        fprintf(stderr, "print_inode: Couldn't format time\n");
    }
    fprintf(stdout, "  %-20s%u | %s\n", "uint32_t mtime", node->mtime, buffer);
    tmp_time = (time_t)node->ctime;
    tm_info = localtime(&tmp_time);
    if(strftime(buffer, PRINT_BUFF_SIZE, "%a %b %d %H:%M:%S %Y", tm_info) == 0)
    {
        fprintf(stderr, "print_inode: Couldn't format time\n");
    }
    fprintf(stdout, "  %-20s%u | %s\n", "uint32_t ctime", node->ctime, buffer);
    fprintf(stdout, "\nDirect zones:\n");
    int i = 0;
    for(; i < DIRECT_ZONES; i++)
    {
        fprintf(stdout, "\tzone[%d] =     %-10u\n", i, node->zone[i]);
    }
    fprintf(stdout, "  %-20s%u\n", "uint32_t indirect", node->indirect);
    fprintf(stdout, "  %-20s%u\n", "uint32_t double", node->two_indirect);
}