#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "sort_impl.h"
#include "timsort.h"
#include "list_sort.h"

typedef struct {
    struct list_head list;
    int val;
    int seq;
} element_t;

#define SAMPLES 1000000

static void create_sample(struct list_head *head, element_t *space, int samples, int mode)
{
    printf("Creating sample\n");

    for (int i = 0; i < samples; i++) {
        element_t *elem = space + i;
        switch (mode) {
            case 0:
                elem->val = rand();
                break;
            case 1:
                elem->val = (i < (samples * 3) / 4) ? i : rand();
                break;
            case 2: 
                elem->val = (i % 20 == 0) ? rand() : i;
                break;
        }
        elem->seq = i;
        list_add_tail(&elem->list, head);
    }
}

static void copy_list(struct list_head *from,
                      struct list_head *to,
                      element_t *space)
{
    if (list_empty(from))
        return;

    element_t *entry;
    list_for_each_entry (entry, from, list) {
        element_t *copy = space++;
        copy->val = entry->val;
        copy->seq = entry->seq;
        list_add_tail(&copy->list, to);
    }
}

int compare(void *priv, const struct list_head *a, const struct list_head *b)
{
    if (a == b)
        return 0;

    int res = list_entry(a, element_t, list)->val -
              list_entry(b, element_t, list)->val;

    if (priv)
        *((int *) priv) += 1;

    return res;
}

bool check_list(struct list_head *head, int count)
{
    if (list_empty(head))
        return 0 == count;

    element_t *entry, *safe;
    size_t ctr = 0;
    list_for_each_entry_safe (entry, safe, head, list) {
        ctr++;
    }
    int unstable = 0;
    list_for_each_entry_safe (entry, safe, head, list) {
        if (entry->list.next != head) {
            // printf("%d\t%d\n", entry->val, entry->seq);
            if (entry->val > safe->val) {
                fprintf(stderr, "\nERROR: Wrong order\n");
                return false;
            }
            if (entry->val == safe->val && entry->seq > safe->seq)
                unstable++;
        }
    }
    if (unstable) {
        fprintf(stderr, "\nERROR: unstable %d\n", unstable);
        return false;
    }

    if (ctr != SAMPLES) {
        fprintf(stderr, "\nERROR: Inconsistent number of elements: %ld\n", ctr);
        return false;
    }
    return true;
}



typedef void (*test_func_t)(void *priv,
                            struct list_head *head,
                            list_cmp_func_t cmp);

typedef struct {
    char *name;
    test_func_t impl;
} test_t;

int main(int argc, char *argv[])
{
    struct list_head sample_head, warmdata_head, testdata_head;
    int count;
    int nums = SAMPLES;

    /* Assume ASLR */
    srand((uintptr_t) &main);

    test_t tests[] = {
        {.name = "timsort", .impl = timsort},
        {.name = "listsort", .impl = list_sort},
        {NULL, NULL},
    };
    test_t *test = tests;

    int mode = 0;
    if (argc > 2) {
        if (strcmp(argv[2], "partial") == 0) {
            mode = 1;
        } else if (strcmp(argv[2], "sparse") == 0) {
            mode = 2;
        } else if (strcmp(argv[2], "random") == 0) {
            mode = 0;
        }
    }

    if (argc < 2) {
        fprintf(stderr, "Usage: %s [timsort|list_sort]\n", argv[0]);
        return 1;
    }

    INIT_LIST_HEAD(&sample_head);

    element_t *samples = malloc(sizeof(*samples) * SAMPLES);
    element_t *warmdata = malloc(sizeof(*warmdata) * SAMPLES);
    element_t *testdata = malloc(sizeof(*testdata) * SAMPLES);

    create_sample(&sample_head, samples, nums, mode);

    for (test_t *test = tests; test->impl != NULL; test++) {
        if (strcmp(argv[1], test->name) == 0) {
            printf("==== Testing %s ====\n", test->name);
            /* Warm up */
            INIT_LIST_HEAD(&warmdata_head);
            INIT_LIST_HEAD(&testdata_head);
            copy_list(&sample_head, &testdata_head, testdata);
            copy_list(&sample_head, &warmdata_head, warmdata);
            test->impl(&count, &warmdata_head, compare);

            /* Test */
            count = 0;
            test->impl(&count, &testdata_head, compare);
            printf("  Comparisons:    %d\n", count);
            printf("  List is %s\n",
                check_list(&testdata_head, nums) ? "sorted" : "not sorted");

            break;
        }
    }

    return 0;
}