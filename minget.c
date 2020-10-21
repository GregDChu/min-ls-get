#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "minget.h"
#include "min.h"

int main(int argc, char *argv[])
{
    // Get and set options internally
    uint8_t flags = EMPTY;
    char *args[POSSIBLE_ARGS] = {NULL};
    int opt;
    while((opt = getopt(argc, argv, OPTIONS)) != -1)
    {
        switch(opt)
        {
    // Check for verbose option (-v)
            case V_OPT:
                SET(flags, VERBOSE);
                break;
    // Check for partition option (-p)
            case P_OPT:
    // Verify that partition argument isn't another option
                if(*optarg == '-')
                {
                    fprintf(stderr, USAGE);
                    exit(EXIT_FAILURE);
                }
                SET(flags, PART);
                args[P_ARG] = optarg;
                break;
    // Check for subpartition option (-s)
            case S_OPT:
    // Verify that partition has been set already
                if(!GET(flags, PART))
                {
                    fprintf(stderr, USAGE);
                    exit(EXIT_FAILURE);
                }
                SET(flags, SUBPART);
                args[S_ARG] = optarg;
                break;
    // Check for any invalid options
            default: // '?'
                fprintf(stderr, USAGE);
                exit(EXIT_FAILURE);
        }
    }
    // Check that there are non-flag arguments
    // and that there aren't too many of them
    if(argc <= optind || (argc - optind) > MAX_RAW_ARGS ||
      (argc - optind) < MIN_RAW_ARGS)
    {
        fprintf(stderr, USAGE);
        exit(EXIT_FAILURE);
    }
    // Get remaining arguments
    while(optind < argc)
    {
        if(args[FILE_ARG] == NULL)
        {
            args[FILE_ARG] = argv[optind];
        }
        else if(args[SRC_ARG] == NULL)
        {
            args[SRC_ARG] = argv[optind];
        }
        else
        {
            args[DEST_ARG] = argv[optind];
        }
        optind++;
    }
    // If necessary, validate and convert partition and sub partition
    int p, s;
    int i = 0;
    if(args[P_ARG] != NULL)
    {
        // Verify that user input for p is a number
        for(; i < strlen(args[P_ARG]); i++)
        {
            if(!isdigit(args[P_ARG][i]))
            {
                fprintf(stderr, "Bad input for p. Please enter a number.\n");
                exit(EXIT_FAILURE);
            }
        }
        // Convert p to a number
        p = atoi(args[P_ARG]);
        // Check that p is in range
        if(p < 0 || p > 3)
        {
            fprintf(stderr, "Partition %d out of range. Must be 0-3.\n", p);
            fprintf(stderr, USAGE);
            exit(EXIT_FAILURE);
        }
    }
    i = 0;
    if(args[S_ARG] != NULL)
    {
        // Verify that user input for s is a number
        for(; i < strlen(args[S_ARG]); i++)
        {
            if(!isdigit(args[S_ARG][i]))
            {
                fprintf(stderr, "Bad input for p. Please enter a number.\n");
                exit(EXIT_FAILURE);
            }
        }
        // Convert s to a number
        s = atoi(args[S_ARG]);
        // Check that s is in range
        if(s < 0 || s > 3)
        {
            fprintf(stderr, "Subpartition %d out of range. Must be 0-3.\n", p);
            fprintf(stderr, USAGE);
            exit(EXIT_FAILURE);
        }
    }
    // Load the image, including superblock and the partition if
    // specified
    if(load_image(args[FILE_ARG], flags, (byte)p, (byte)s) == FAIL)
    {
        close_image();
        exit(EXIT_FAILURE);
    }
    // Allocate space for the desired file
    inode file;
    file = calloc(1, sizeof(struct inode));
    if(file == NULL)
    {
        fprintf(stderr, "minls: Cannot allocate space for file data.\n");
        close_image();
        exit(EXIT_FAILURE);
    }
    // Locate the inode of the desired file
    if(path_to_inode(args[SRC_ARG], file) == FAIL)
    {
        free(file);
        close_image();
        return 1;
    }
    // Check if user wanted more info
    if(GET(flags, VERBOSE))
    {
        // Print out more info
        print_sb();
        print_inode(file);
    }
    // Validate that the file is regular
    FILE *out;
    if((file->mode & FILE_TYPE) != REG_FILE)
    {
        // Throw an error if it is not
        fprintf(stderr, "%s: Not a regular file.\n", args[SRC_ARG]);
        close_image();
        free(file);
        exit(EXIT_FAILURE);
    }
    else
    {
        // If user didn't specify a destination
        // set it to stdout
        if(args[DEST_ARG] == NULL)
        {
            out = stdout;
        }
        // Otherwise, open their desired location
        else
        {
            out = fopen(args[DEST_ARG], "r+");
            // Catch any errors
            if(out == NULL)
            {
                perror("minget");
                close_image();
                free(file);
                exit(EXIT_FAILURE);
            }
        }
        // Write the desired file to the desired location
        if(write_contents(file, out) == FAIL)
        {
            // Catch any errors
            close_image();
            free(file);
            fclose(out);
            exit(EXIT_FAILURE);
        }
    }
    // Bookkeeping
    close_image();
    free(file);
    fclose(out);
    // Exit with success status
    exit(EXIT_SUCCESS);
}