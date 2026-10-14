#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

#include <linux/ioctl.h>

#define ASGN1_MAGIC 'A'

#define ASGN1_RESET_BUFFER _IO(ASGN1_MAGIC, 1)
#define ASGN1_GET_STATS _IOR(ASGN1_MAGIC, 2, struct asgn1_stats)
#define ASGN1_SET_MODE _IOW(ASGN1_MAGIC, 3, int)
#define ASGN1_GET_VERSION _IOR(ASGN1_MAGIC, 4, char[64])

struct asgn1_stats {
    int open_count;
    int write_count;
    int read_count;
    int buffer_len;
    int last_write_size;
};


#endif