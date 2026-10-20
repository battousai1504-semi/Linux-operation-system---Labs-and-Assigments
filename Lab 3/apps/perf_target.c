#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE 100000
#define ITERATIONS 500

static volatile int search_sink;

double heavy_compute(double *array, int size)
{
    double result = 0.0;

    for (int i = 0; i < size; i++)
        result += sqrt(array[i]) * log(array[i] + 1.0);

    return result;
}

int linear_search(int *array, int size, int target)
{
    for (int i = 0; i < size; i++) {
        if (array[i] == target)
            return i;
    }

    return -1;
}

void io_intensive(int count)
{
    FILE *file = fopen("/tmp/lab03_perf_test.txt", "w");

    if (!file) {
        perror("fopen");
        return;
    }

    for (int i = 0; i < count; i++)
        fprintf(file, "Line %d: measurement data\n", i);

    fclose(file);
    remove("/tmp/lab03_perf_test.txt");
}

int main(void)
{
    double *double_array;
    int *integer_array;
    double total = 0.0;

    printf("=== Perf Target App - LAB03 ===\n");

    double_array = malloc(sizeof(*double_array) * ARRAY_SIZE);
    integer_array = malloc(sizeof(*integer_array) * ARRAY_SIZE);

    if (!double_array || !integer_array) {
        free(double_array);
        free(integer_array);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < ARRAY_SIZE; i++) {
        double_array[i] = i + 1.0;
        integer_array[i] = i;
    }

    printf("[1] Running heavy computation...\n");

    for (int iteration = 0; iteration < ITERATIONS; iteration++)
        total += heavy_compute(double_array, ARRAY_SIZE);

    printf("[2] Running linear searches...\n");

    for (int i = 0; i < 200; i++) {
        search_sink += linear_search(
            integer_array,
            ARRAY_SIZE,
            ARRAY_SIZE - 1
        );
    }

    printf("[3] Writing test file...\n");
    io_intensive(10000);

    printf("Result: %.3f\n", total);
    printf("Search sink: %d\n", search_sink);

    free(double_array);
    free(integer_array);

    return EXIT_SUCCESS;
}
