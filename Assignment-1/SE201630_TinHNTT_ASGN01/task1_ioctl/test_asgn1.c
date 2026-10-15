
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include "asgn1_ioctl.h"

int main(void)
{
    int fd;
    int mode = 0;
    char data[] = "Hello Linux";
    char read_buf[128] = {0};
    char version[64] = {0};
    struct asgn1_stats stats;

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    /* Reset buffer */
    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0)
        perror("RESET_BUFFER");

    /* Set mode 0 */
    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0)
        perror("SET_MODE");

    /* Write data */
    if (write(fd, data, strlen(data)) < 0)
        perror("write");

    /* Read data */
    if (read(fd, read_buf, sizeof(read_buf) - 1) < 0)
        perror("read");
    else
        printf("Read data: %s\n", read_buf);

    /* Get statistics */
    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("GET_STATS");
    } else {
        printf("Open count: %d\n", stats.open_count);
        printf("Write count: %d\n", stats.write_count);
        printf("Read count: %d\n", stats.read_count);
        printf("Buffer length: %d\n", stats.buffer_len);
        printf("Last write size: %d\n", stats.last_write_size);
    }

    /* Get version */
    if (ioctl(fd, ASGN1_GET_VERSION, version) < 0)
        perror("GET_VERSION");
    else
        printf("Version: %s\n", version);

        /* Reset after writing data */
    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("RESET_BUFFER after write");
        close(fd);
        return 1;
    }

    /* Verify counters and buffer length */
    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("GET_STATS after reset");
        close(fd);
        return 1;
    }

    if (stats.write_count != 0 ||
        stats.read_count != 0 ||
        stats.buffer_len != 0) {
        fprintf(stderr, "FAIL: RESET did not clear stats\n");
        close(fd);
        return 1;
    }

    /* Reopen to reset the file offset */
    close(fd);
    fd = open("/dev/asgn1", O_RDONLY);

    if (fd < 0) {
        perror("open after reset");
        return 1;
    }

    ssize_t n = read(fd, read_buf, sizeof(read_buf));

    if (n != 0) {
        fprintf(stderr, "FAIL: Buffer is not empty\n");
        close(fd);
        return 1;
    }

    printf("PASS: RESET cleared buffer and counters\n");

    close(fd);

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open for Echo mode");
        return 1;
    }

    mode = 1;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("SET_MODE Echo");
        close(fd);
        return 1;
    }

    if (write(fd, data, strlen(data)) != (ssize_t)strlen(data)) {
        fprintf(stderr, "FAIL: Echo write\n");
        close(fd);
        return 1;
    }

    memset(read_buf, 0, sizeof(read_buf));

    n = read(fd, read_buf, sizeof(read_buf) - 1);

    if (n != (ssize_t)strlen(data) ||
        memcmp(read_buf, data, strlen(data)) != 0) {
        fprintf(stderr, "FAIL: Echo mode data mismatch\n");
        close(fd);
        return 1;
    }

    printf("PASS: Echo mode - %s\n", read_buf);

    errno = 0;

    if (ioctl(fd, 0) != -1 || errno != ENOTTY) {
        fprintf(stderr, "FAIL: Invalid ioctl handling\n");
        close(fd);
        return 1;
    }

    printf("PASS: Invalid ioctl returns ENOTTY\n");

    close(fd);
    return 0;
}
