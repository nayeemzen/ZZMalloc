/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "0xdefec8ed",
    /* First member's full name */
    "Zaid Al Khishman",
    /* First member's email address */
    "zaid.al.khishman@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Nayeem Husain Zen",
    /* Second member's email address (leave blank if none) */
    "nayeem.zen@mail.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))
#define MIN(x,y) ((x) < (y)?(x): (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

void* heap_listp = NULL;

/* Implementation data structures */
typedef struct list_block {
    struct list_block *prev;
    struct list_block *next;
} list_block;

/* Implementation globals and macros */
#define NUM_LISTS 8
#define MIN_BLOCK_SIZE 2 * DSIZE

// allow configuring debug via commandline -DDBG
#ifndef DBG
#define DBG 0
#endif

#define DBG_PRINT(...) DBG ? printf(__VA_ARGS__): (void)NULL;
#define DBG_ASSERT(expr) DBG ? assert(expr): (void)NULL;

// Global segregated lists of different size classes
list_block *seg_lists[NUM_LISTS];

/* Implementation functions */
void seg_list_init(void) {
    int i;
    for(i = 0; i < NUM_LISTS; i++) {
        seg_lists[i] = NULL;
    }
}

// TODO(Zen): Improve from O(n) -> O(1)
int calc_size_class(size_t sz) {
    DBG_ASSERT(sz >= MIN_BLOCK_SIZE);
    DBG_ASSERT(sz % 16 == 0);

    int i = 0, bucket_sz = MIN_BLOCK_SIZE;
    while(sz > bucket_sz && i < NUM_LISTS) {
        bucket_sz = bucket_sz << 1;
        i++;
    }

    return MIN(i, NUM_LISTS - 1);
}

void seg_list_print(void) {
#ifdef DBG
    int i;
    for (i = 0; i < NUM_LISTS; i++) {
        DBG_PRINT("printing seg_lists[%d]\n", i);
        list_block *ls = seg_lists[i];
        if (!ls) continue;
        do {
            DBG_PRINT("\tblock of size: %lu @ 0x%p\n", GET_SIZE(HDRP((void*)ls)), (void*)ls);
            ls = ls->next;
        } while (ls != seg_lists[i]); 
    }
#endif
}

void seg_list_add(list_block* bp) {
    int bsize = GET_SIZE(HDRP(bp));
    // Get the size class for block of bsize;
    int sz_cls = calc_size_class(bsize);
    list_block *list = seg_lists[sz_cls];
    
    DBG_PRINT("Inserting to seg_lists[%d] @ 0x%p, block size: %d\n", sz_cls, (void*)list, bsize);
    if (!list) {
        DBG_PRINT("list@%d is empty!!\n", sz_cls);
        seg_lists[sz_cls] = bp;
        seg_lists[sz_cls]->next = bp;
        seg_lists[sz_cls]->prev = bp;
        seg_list_print();
        return;
    }
    
    // seg list is not empty, insert bp at head
    bp->next = seg_lists[sz_cls];
    bp->prev = seg_lists[sz_cls]->prev;
    bp->prev->next = bp;
    bp->next->prev = bp;

#ifdef DBG
    seg_list_print();
#endif
}

void seg_list_remove(list_block* blk) {
    if (!blk) return;
    size_t sz = GET_SIZE(HDRP(blk));
    int sz_cls = calc_size_class(sz);
    if (blk != blk->next) {
        if (blk->prev && blk->next) {
            blk->prev->next = blk->next;
            blk->next->prev = blk->prev;
        }
        
        // if blk is the head of the list
        if (blk == seg_lists[sz_cls]) {
            seg_lists[sz_cls] = blk->next;
        }

        return;
    }

    seg_lists[sz_cls] = NULL;
}

void * seg_list_find_fit(size_t sz) {
    int sz_cls;
    for(sz_cls = calc_size_class(sz); sz_cls < NUM_LISTS; sz_cls++) {
        list_block *blk = seg_lists[sz_cls];
        if (!blk) continue;
        do {
            size_t blk_sz = GET_SIZE(HDRP(blk));
            // Search for blocks that are >= sz
            if (blk_sz < sz) {
                blk = blk->next;
                continue;
            }

            size_t rem_size = blk_sz - sz;
            //TODO(Zen): Remove repeated code (repeated in place)
            // Can't be split to produce another free block
            if (rem_size < MIN_BLOCK_SIZE) {
                seg_list_remove(blk);
                return (void*)blk;
            }
            
            // Split block and put excess fragment into appropriate size class
            // Remove block from current size_class
            seg_list_remove(blk);
            PUT(HDRP(blk), PACK(sz, 1));
            PUT(FTRP(blk), PACK(sz, 1)); 

            // Mark next block of size rem_size as empty and add to seg_list
            PUT(HDRP(NEXT_BLKP(blk)), PACK(rem_size, 0));
            PUT(FTRP(NEXT_BLKP(blk)), PACK(rem_size, 0));
            seg_list_add((list_block*)NEXT_BLKP(blk));
            
            return blk;
        } while (blk != seg_lists[sz_cls]); 
    }

    return NULL;
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
   if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
         return -1;
     PUT(heap_listp, 0);                         // alignment padding
     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
     PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
     heap_listp += DSIZE;
     
     seg_list_init();
     return 0;
 }

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        seg_list_remove((list_block*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        seg_list_remove((list_block*)PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        seg_list_remove((list_block*)PREV_BLKP(bp));
        seg_list_remove((list_block*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));        // new epilogue header

    /* Coalesce if the previous block was free */
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));
  DBG_ASSERT(bsize >= asize);
  DBG_ASSERT(asize % 16 == 0);
  
  size_t rem_size = bsize - asize;
  if (rem_size < MIN_BLOCK_SIZE) {
      DBG_PRINT("Could not split\n");
      PUT(HDRP(bp), PACK(bsize, 1));
      PUT(FTRP(bp), PACK(bsize, 1));
      return;
  }
  
  // Can successfully split, allocate block of asize
  PUT(HDRP(bp), PACK(asize, 1));
  PUT(FTRP(bp), PACK(asize, 1)); 
  
  // Mark next block of size rem_size as empty and add to seg_list
  PUT(HDRP(NEXT_BLKP(bp)), PACK(rem_size, 0));
  PUT(FTRP(NEXT_BLKP(bp)), PACK(rem_size, 0));
  seg_list_add((list_block*)NEXT_BLKP(bp));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{   
    if(bp == NULL){
      return;
    }
    
    DBG_PRINT("Free request for 0x%p size of %lu\n", bp, GET_SIZE(HDRP(bp)));
    
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    seg_list_add(coalesce(bp));
}


/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;
    
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    
    DBG_PRINT("Malloc request size: %lu\n", asize);
    /* Search the free list for a fit */
    if ((bp = seg_list_find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
  return 1;
}
