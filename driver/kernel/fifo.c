#if !SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif /* SONIC_KERNEL */
#include "sonic.h"
#include "fifo.h"

struct sonic_fifo *alloc_fifo (int size, int exp, int * stopper)
{
    int i, max_num_entry=0;
    struct sonic_fifo *fifo;
    struct sonic_fifo_entry *entry;
    SONIC_DDPRINT("size = %d exp = %d\n", size, exp);

    // Allocate a page...
    fifo = (struct sonic_fifo *) ALLOC_PAGE();
    if (!fifo) {
        SONIC_ERROR("alloc failed\n");
        return NULL;
    }

    max_num_entry = PAGE_SIZE - (((char *) (fifo->entry)) - ((char *) fifo))
                / sizeof(struct sonic_fifo_entry);

    if (size > max_num_entry) {
        SONIC_ERROR("size cannot be larger than %d, and set to %d\n",
            max_num_entry, max_num_entry);
        size = max_num_entry;
    }

    fifo->size = size;
    fifo->w_tail = 0;
    fifo->r_head = 0;
    fifo->exp = exp;
    fifo->entry_size = order2bytes(exp);
    fifo->stopper = stopper;

    for ( i = 0 ; i < size ; i ++ ) {
        entry = &fifo->entry[i];
        entry->flag = 0;
        entry->data = ALLOC_PAGES(exp);
        memset(entry->data, 0, order2bytes(exp));        // FIXME
        if(!entry->data) {
            SONIC_ERROR("alloc failed\n");
            fifo->size = i;
            goto abort;
        }
    }

    return fifo;

abort:
    free_fifo(fifo);

    return NULL;
}

void free_fifo (struct sonic_fifo *fifo)
{
    int i;
    struct sonic_fifo_entry *entry;
    SONIC_DDPRINT("\n");

    for ( i = 0 ; i < fifo->size ; i ++ ) {
        entry = &fifo->entry[i];
        FREE_PAGES(entry->data, fifo->exp);
    }

    FREE_PAGE(fifo);
}

void reset_fifo(struct sonic_fifo *fifo)
{
    int i;

    fifo->w_tail = 0;
    fifo->r_head = 0;

    for ( i = 0 ; i < fifo->size ; i ++) 
        fifo->entry[i].flag = 0;
}
