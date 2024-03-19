#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "list.h"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

typedef int __attribute__((nonnull(2,3))) (*list_cmp_func_t)(void *,
		const struct list_head *, const struct list_head *);

__attribute__((nonnull(2,3,4)))
static struct list_head *merge_list_sort(void *priv, list_cmp_func_t cmp,
				struct list_head *a, struct list_head *b)
{
	struct list_head *head, **tail = &head;

	for (;;) {
		/* if equal, take 'a' -- important for sort stability */
		if (cmp(priv, a, b) <= 0) {
			*tail = a;
			tail = &a->next;
			a = a->next;
			if (!a) {
				*tail = b;
				break;
			}
		} else {
			*tail = b;
			tail = &b->next;
			b = b->next;
			if (!b) {
				*tail = a;
				break;
			}
		}
	}
	return head;
}

__attribute__((nonnull(2,3,4,5)))
static void merge_final_list_sort(void *priv, list_cmp_func_t cmp, struct list_head *head,
			struct list_head *a, struct list_head *b)
{
	struct list_head *tail = head;
	int count = 0;

	for (;;) {
		/* if equal, take 'a' -- important for sort stability */
		if (cmp(priv, a, b) <= 0) {
			tail->next = a;
			a->prev = tail;
			tail = a;
			a = a->next;
			if (!a)
				break;
		} else {
			tail->next = b;
			b->prev = tail;
			tail = b;
			b = b->next;
			if (!b) {
				b = a;
				break;
			}
		}
	}

	/* Finish linking remainder of list b on to tail */
	tail->next = b;
	do {
		/*
		 * If the merge is highly unbalanced (e.g. the input is
		 * already sorted), this loop may run many iterations.
		 * Continue callbacks to the client even though no
		 * element comparison is needed, so the client's cmp()
		 * routine can invoke cond_resched() periodically.
		 */
		if (unlikely(!++count))
			cmp(priv, b, b);
		b->prev = tail;
		tail = b;
		b = b->next;
	} while (b);

	/* And the final links to make a circular doubly-linked list */
	tail->next = head;
	head->prev = tail;
}

__attribute__((nonnull(2,3)))
void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
	struct list_head *list = head->next, *pending = NULL;
	size_t count = 0;	/* Count of pending */

	if (list == head->prev)	/* Zero or one elements */
		return;

	/* Convert to a null-terminated singly-linked list. */
	head->prev->next = NULL;

	do {
		size_t bits;
		struct list_head **tail = &pending;

		/* Find the least-significant clear bit in count */
		for (bits = count; bits & 1; bits >>= 1)
			tail = &(*tail)->prev;
		/* Do the indicated merge */
		if (likely(bits)) {
			struct list_head *a = *tail, *b = a->prev;

			a = merge_list_sort(priv, cmp, b, a);
			/* Install the merged result in place of the inputs */
			a->prev = b->prev;
			*tail = a;
		}

		/* Move one element from input list to pending */
		list->prev = pending;
		pending = list;
		list = list->next;
		pending->next = NULL;
		count++;
	} while (list);

	/* End of input; merge together all the pending lists. */
	list = pending;
	pending = pending->prev;
	for (;;) {
		struct list_head *next = pending->prev;

		if (!next)
			break;
		list = merge_list_sort(priv, cmp, pending, list);
		pending = next;
	}
	/* The final merge, rebuilding prev links */
	merge_final_list_sort(priv, cmp, head, pending, list);
}