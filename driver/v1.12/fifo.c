#if !SONIC_KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif /* SONIC_KERNEL */
#include "sonic.h"
#include "fifo.h"

struct sonic_fifo *alloc_fifo (int size, int porder, int * stopper)
{
	int i; 
	struct sonic_fifo *fifo;
	struct sonic_fifo_entry *entry;
	SONIC_DDPRINT("size = %d porder = %d\n", size, porder);

	fifo = ALLOC(sizeof(struct sonic_fifo));
	if (!fifo) {
		SONIC_ERROR("alloc failed\n");
		return NULL;
	}
	SONIC_MDPRINT(fifo);

	if (size > FIFO_NUM_ENTRY) {
		SONIC_ERROR("size cannot be larger than %d, and set to %d\n",
			FIFO_NUM_ENTRY, FIFO_NUM_ENTRY);
		size = FIFO_NUM_ENTRY;
	}

	fifo->size = size;
	fifo->w_tail = 0;
	fifo->r_head = 0;
	fifo->porder = porder;
	fifo->stopper = stopper;

	SONIC_DDPRINT("fifo porder = %d size = %d\n", porder, size);

	for ( i = 0 ; i < size ; i ++ ) {
		entry = &fifo->entry[i];
		entry->flag = 0;
//#ifdef SONIC_NO_ALLOC
		entry->data = ALLOC_PAGES2(porder);
		SONIC_MDPRINT(entry->data);
		memset(entry->data, 0, order2bytes(porder));	// FIXME
		if(!entry->data) {
			SONIC_ERROR("alloc failed\n");
			fifo->size = i;
			goto abort;
		}
//#endif /* SONIC_NO_ALLOC */
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
//#ifdef SONIC_NO_ALLOC
		FREE_PAGES(entry->data, fifo->porder);
//#endif /* SONIC_NO_ALLOC */
	}

	FREE(fifo);
}

void reset_fifo(struct sonic_fifo *fifo)
{
	int i;
//	int esize = fifo_entry_size(fifo);

	fifo->w_tail = 0;
	fifo->r_head = 0;

	for ( i = 0 ; i < fifo->size ; i ++) {
//		memset(fifo->entry[i].data, 0, esize);
		fifo->entry[i].flag = 0;
	}
}

int fifo_entry_size( struct sonic_fifo *fifo)
{
	return order2bytes(fifo->porder);
}
