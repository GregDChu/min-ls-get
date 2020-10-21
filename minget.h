#ifndef MINGET_H
#define MINGET_H

#define OPTIONS "vp:s:"
#define V_OPT 'v'
#define P_OPT 'p'
#define S_OPT 's'
#define USAGE "Usage: minget [ -v] [ -p part [-s subpart ] ] \
imagefile srcpath [ dstpath ]\n"

#define VERBOSE 0x01
#define PART 0x02
#define SUBPART 0x04
#define EMPTY 0x00
#define SET(FLAGS, BIT) (FLAGS |= BIT)
#define CLEAR(FLAGS, BIT) (FLAGS &= (~BIT))
#define GET(FLAGS, BIT) (FLAGS & BIT)

#define P_ARG 0
#define S_ARG 1
#define FILE_ARG 2
#define SRC_ARG 3
#define DEST_ARG 4
#define POSSIBLE_ARGS 5
#define MAX_RAW_ARGS 3
#define MIN_RAW_ARGS 2

#endif