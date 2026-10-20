#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void process_data(const int *data, int len)
{
    int sum = 0;

    /* Fix 1: valid indexes are 0 through len - 1. */
    for (int i = 0; i < len; i++)
        sum += data[i];

    printf("Sum = %d\n", sum);
}

char *create_message(const char *text)
{
    char *buf = malloc(strlen(text) + 1);

    if (!buf)
        return NULL;

    strcpy(buf, text);

    /*
     * Fix 2: do not free buf here.
     * Ownership is transferred to the caller.
     */
    return buf;
}

long long calculate(int a, int b)
{
    /*
     * Fix 3: convert before multiplication so the operation
     * is performed using 64-bit arithmetic.
     */
    long long result = (long long)a * b;

    printf(
        "calculate(%d, %d) = %lld\n",
        a,
        b,
        result
    );

    return result;
}

int main(void)
{
    int arr[5] = {10, 20, 30, 40, 50};
    char *msg;

    printf("=== Fixed App - LAB03 ===\n");

    printf("[TEST1] Array processing\n");
    process_data(arr, 5);

    printf("[TEST2] Creating message\n");
    msg = create_message("Hello Embedded Linux");

    if (!msg) {
        fprintf(stderr, "Failed to allocate message\n");
        return EXIT_FAILURE;
    }

    printf("Message: %s\n", msg);
    free(msg);

    printf("[TEST3] Integer calculation\n");
    calculate(100000, 100000);

    printf("Done.\n");
    return EXIT_SUCCESS;
}
