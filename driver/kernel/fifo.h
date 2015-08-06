#ifndef __SONIC_FIFO__
#define __SONIC_FIFO__
#include "sonic.h"

#if !SONIC_KERNEL
#include <stdlib.h>
#endif /* SONIC_KERNEL */

//#define SONIC_NO_ALLOC

struct sonic_fifo_entry {
    int flag;
    int reserved[13];
    void * data;
};

// fifo size is a single page
struct sonic_fifo {
    int size;           // number of entry to use
    int exp;            // entry->data size
    int entry_size;
    int reserved0;
    int * stopper;
    int reserved1[10];
    int w_tail;
    int reserved2[15];
    int r_head;
    int reserved3[15];
    struct sonic_fifo_entry entry[0];
};

#ifdef SONIC_NO_ALLOC
static inline void * get_write_entry(struct sonic_fifo *fifo) 
{
    int *flag = &fifo->entry[fifo->w_tail].flag;
    const volatile int * const stopper = fifo->stopper;

    while (__sync_and_and_fetch(flag, 1) > 0 && *stopper == 0)
        ;
    if (*stopper)
        return NULL;
    return fifo->entry[fifo->w_tail].data;
}

static inline void put_write_entry(struct sonic_fifo *fifo, void *data)
{
    int *flag = &fifo->entry[fifo->w_tail].flag;
    __sync_fetch_and_add(flag, 1);
    fifo->w_tail = (fifo->w_tail + 1) % fifo->size;
}

static inline void * get_read_entry(struct sonic_fifo *fifo)
{
    int *flag = &fifo->entry[fifo->r_head].flag;
    const volatile int * const stopper = fifo->stopper;

    while (__sync_and_and_fetch(flag, 1) == 0 && *stopper == 0)
        ;
    if (*stopper)
        return NULL;
    return fifo->entry[fifo->r_head].data;
}

static inline void put_read_entry(struct sonic_fifo *fifo, void *data)
{
    int *flag = &fifo->entry[fifo->r_head].flag;
    __sync_fetch_and_sub(flag, 1);
    fifo->r_head = (fifo->r_head+1) % fifo->size;
}
#else /* SONIC_NO_ALLOC */
// SWAP
static inline void * get_write_entry(struct sonic_fifo *fifo) 
{
    int *flag = &fifo->entry[fifo->w_tail].flag;
    const volatile int * const stopper = fifo->stopper;

    while (__sync_and_and_fetch(flag, 1) > 0 && *stopper == 0)
        ;
    if (*stopper)
        return NULL;

    return fifo->entry[fifo->w_tail].data;
}

static inline void put_write_entry(struct sonic_fifo *fifo, void *data)
{
    int *flag = &fifo->entry[fifo->w_tail].flag;
    fifo->entry[fifo->w_tail].data = data;          // TODO
    __sync_fetch_and_add(flag, 1);
    fifo->w_tail = (fifo->w_tail + 1) % fifo->size;
}

static inline void * get_read_entry(struct sonic_fifo *fifo)
{
    int *flag = &fifo->entry[fifo->r_head].flag;
    const volatile int * const stopper = fifo->stopper;

    while (__sync_and_and_fetch(flag, 1) == 0 && *stopper == 0)
        ;
    if (*stopper)
        return NULL;
    return fifo->entry[fifo->r_head].data;
}

static inline void put_read_entry(struct sonic_fifo *fifo, void *data)
{
    int *flag = &fifo->entry[fifo->r_head].flag;
    fifo->entry[fifo->r_head].data = data;              // TODO
    __sync_fetch_and_sub(flag, 1);
    fifo->r_head = (fifo->r_head+1) % fifo->size;
}

// FIFO swapping
static inline void push_entry(struct sonic_fifo *in_fifo, struct sonic_fifo *out_fifo, void *data)
{
    void * tdata = data;
    if (out_fifo) {
        tdata = get_write_entry(out_fifo);
        if (!tdata) {
            tdata = data;
            goto out;
        }

        put_write_entry(out_fifo, data);
    }

out:
    put_read_entry(in_fifo, tdata);
}

#endif /* SONIC_NO_ALLOC */

/* for debugging only */
static inline void * get_write_entry_no_block(struct sonic_fifo *fifo)
{
    return fifo->entry[fifo->w_tail].data;
}

static inline void put_write_entry_no_block(struct sonic_fifo *fifo)
{
    fifo->w_tail = (fifo->w_tail + 1) % fifo->size;
}

static inline void * get_read_entry_no_block(struct sonic_fifo *fifo)
{
    return fifo->entry[fifo->r_head].data;
}

static inline void put_read_entry_no_block(struct sonic_fifo *fifo)
{
    fifo->r_head = (fifo->r_head + 1) % fifo->size;
}

struct sonic_fifo * alloc_fifo (int, int, int *);
void free_fifo (struct sonic_fifo *);
void reset_fifo (struct sonic_fifo *);
#endif /* __SONIC_FIFO__ */
