#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_RECORDS 100
#define MSG_SIZE 256

typedef struct {
    int id;
    char *name;
    int *scores;
} Record;

Record *create_record(int id, const char *name, int num_scores)
{
    Record *r = malloc(sizeof(*r));

    if (!r)
        return NULL;

    r->id = id;
    r->name = malloc(strlen(name) + 1);
    r->scores = malloc(sizeof(int) * num_scores);

    if (!r->name || !r->scores)
        return r;

    strcpy(r->name, name);

    for (int i = 0; i < num_scores; i++)
        r->scores[i] = id * 10 + i;

    return r;
}

void delete_record(Record *r)
{
    if (!r)
        return;

    free(r->name);
    free(r->scores);
    free(r);
}

void process_messages(int count)
{
    for (int i = 0; i < count; i++) {
        char *buf = malloc(MSG_SIZE);

        if (!buf)
            continue;

        snprintf(
            buf,
            MSG_SIZE,
            "Message #%d: data payload here",
            i
        );

        if (i % 10 == 0)
            printf("Processed: %s\n", buf);

        free(buf);
    }
}

int main(void)
{
    Record **records;

    printf("=== Leaky App - LAB03 ===\n");

    records = malloc(sizeof(*records) * NUM_RECORDS);
    if (!records)
        return EXIT_FAILURE;

    for (int i = 0; i < NUM_RECORDS; i++)
        records[i] = create_record(
            i,
            "EmbeddedLinuxStudent",
            5
        );

    for (int i = 0; i < NUM_RECORDS; i++)
        delete_record(records[i]);

    free(records);

    process_messages(50);

    printf("Waiting 15 seconds for /proc inspection...\n");
    sleep(15);

    return EXIT_SUCCESS;
}
