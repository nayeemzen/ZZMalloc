/*
 * This implementation builds on the implicit list implementation
 * provided in the textbook to build a segregated lists based
 * allocator.
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are coalesced and are reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 * By default this implementation uses 11 buckets of  different sizes
 * of free blocks, although this is configurable via preprocessor macros
 * We use a first fit policy when attempting to reuse a block 
 * from the free lists.
 * We always  insert free blocks to the free lists at the
 * beginning of the appropriate free list
 * structure of a free block is [Header][Previous][Next][empty/optional][Footer]
 * structure of allocated block is [Header][Payload][Footer]
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

// allow configuration of those via command line
#ifndef CHSIZE 
#define CHSIZE 8
#endif
#define CHUNKSIZE   (1<<CHSIZE)      /* initial heap size (bytes) */

#ifndef NUM_LISTS
#define NUM_LISTS 11
#endif

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
#define MIN_BLOCK_SIZE 2 * DSIZE

// allow configuring debug via commandline -DDBG
#ifndef DBG
#define DBG_PRINT(...)       (void)NULL;
#define DBG_ASSERT(expr)     (void)NULL;
#define SEG_LIST_PRINT(...)  (void)NULL;
#define DBG_PRINT_HEAP(...)  (void)NULL;
#else
#define DBG_PRINT(...)       printf(__VA_ARGS__);
#define DBG_ASSERT(expr)     assert(expr);
#define SEG_LIST_PRINT(...)  seg_list_print(__VA_ARGS__);
#define DBG_PRINT_HEAP(...)  print_heap(__VA_ARGS__);
#endif



// Global segregated lists of different size classes
list_block *seg_lists[NUM_LISTS];

/* used for debugging */
void* epilogue = NULL;
void* start_of_heap = NULL;

void* prologue = NULL;

/* Implementation functions */
void seg_list_init(void) {
    int i;
    for(i = 0; i < NUM_LISTS; i++) {
        seg_lists[i] = NULL;
    }
}

// TODO(Zen): Improve from O(n) -> O(1)
// I don't think that can be done, best we can do is O(logn)
// by doing a binary search to find the appropriate list
// But it's actually already O(1) because NUM LISTS is a constant
/**********************************************************
 * calc_size_class
 * find the ideal free list to which a block of a given
 * size should belong
 **********************************************************/
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


/**********************************************************
 * seg_list_print
 * print every block in every free list
 **********************************************************/
void seg_list_print(void) {
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

}

/**********************************************************
 * seg_list_add
 * add a block to the free lists
 * the block is always inserted at 
 * the head of the appropriate list
 **********************************************************/
void seg_list_add(list_block* bp) {
    int bsize = GET_SIZE(HDRP(bp));
    // Get the size class for block of bsize;
    int sz_cls = calc_size_class(bsize);
    list_block *list = seg_lists[sz_cls];


    DBG_ASSERT(GET(HDRP(bp)) == GET(FTRP(bp)));
    DBG_ASSERT(bsize >= 2*DSIZE);

    DBG_PRINT("Inserting to seg_lists[%d] @ 0x%p, block size: %d\n", sz_cls, (void*)list, bsize);
    if (!list) {
        DBG_PRINT("list@%d is empty!!\n", sz_cls);
        seg_lists[sz_cls] = bp;
        bp->next = bp;
        bp->prev = bp;
        SEG_LIST_PRINT();
        return;
    }

    // seg list is not empty, insert bp at head
    bp->next = seg_lists[sz_cls];
    bp->prev = seg_lists[sz_cls]->prev;
    bp->prev->next = bp;
    bp->next->prev = bp;
    SEG_LIST_PRINT();
}

/**********************************************************
 * seg_list_remove
 * remove a block from the free lists
 **********************************************************/
void seg_list_remove(list_block* blk) {
    if (!blk) return;
    size_t sz = GET_SIZE(HDRP(blk));

    DBG_ASSERT(FTRP(blk) > HDRP(blk));
    DBG_PRINT("blk at %p, header = %p, footer = %p\n", blk,*((unsigned long*) HDRP(blk)),*((unsigned long*)FTRP(blk)));
    DBG_PRINT("size from header = %d,size from footer = %d\n", GET_SIZE(HDRP(blk)), GET_SIZE(FTRP(blk)));
    DBG_ASSERT(GET(HDRP(blk)) == GET(FTRP(blk)));

    int sz_cls = calc_size_class(sz);
    DBG_ASSERT(sz >= 2*DSIZE);
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

/**********************************************************
 * seg_list_find_split_place
 * finds a block (on a first fit policy) big enough for sz
 * splits it of possible
 * places userload on higher part of the old block
 * (rem size forms the lower block) 
 * returns pointer to user load block
 **********************************************************/
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
            void* usrptr = (void*)blk + rem_size;
            PUT(HDRP(usrptr), PACK(sz, 1));
            PUT(FTRP(usrptr), PACK(sz, 1)); 
            DBG_ASSERT(FTRP(blk) > HDRP(blk));

            // Mark next block of size rem_size as empty and add to seg_list
            PUT(HDRP(blk), PACK(rem_size, 0));
            PUT(FTRP(blk), PACK(rem_size, 0));
            seg_list_add((list_block*)blk);
            
            return usrptr;
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
     DBG_PRINT("using NUM_LISTS=%d, CHSIZE=%d\n",NUM_LISTS,CHSIZE);
     if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
         return -1;

     start_of_heap = heap_listp;

     PUT(heap_listp, 0);                         // alignment padding
     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer

     prologue = (void*) (heap_listp + (2 * WSIZE));

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
    DBG_ASSERT(FTRP(bp) > HDRP(bp));
    DBG_ASSERT(FTRP(PREV_BLKP(bp)) > HDRP(PREV_BLKP(bp)));

    if(NEXT_BLKP(bp) != epilogue){
        DBG_PRINT("blk at %p\n",NEXT_BLKP(bp));
        DBG_ASSERT(FTRP(NEXT_BLKP(bp)) > HDRP(NEXT_BLKP(bp)));
    }

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        seg_list_remove((list_block*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        DBG_ASSERT(FTRP(bp) > HDRP(bp));
        DBG_ASSERT(bp > start_of_heap);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        seg_list_remove((list_block*)PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        DBG_ASSERT(FTRP(bp) > HDRP(bp));
        DBG_ASSERT(FTRP(PREV_BLKP(bp)) > HDRP(PREV_BLKP(bp)));

        DBG_ASSERT((void*)PREV_BLKP(bp) > start_of_heap);
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        seg_list_remove((list_block*)PREV_BLKP(bp));
        seg_list_remove((list_block*)NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));

        if(NEXT_BLKP(bp) != epilogue)
        DBG_ASSERT(FTRP(NEXT_BLKP(bp)) > HDRP(NEXT_BLKP(bp)));

        DBG_ASSERT(FTRP(PREV_BLKP(bp)) > HDRP(PREV_BLKP(bp)));
        DBG_ASSERT(FTRP(bp) > HDRP(bp));

        DBG_ASSERT((void*)PREV_BLKP(bp) > start_of_heap);
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

    epilogue = NEXT_BLKP(bp);
    DBG_ASSERT(FTRP(bp) > HDRP(bp));

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
      DBG_PRINT("Could not split, required_size:%d, block_size:%d, remaining_size:%d\n", asize, bsize, rem_size);
      PUT(HDRP(bp), PACK(bsize, 1));
      PUT(FTRP(bp), PACK(bsize, 1));
      DBG_ASSERT(FTRP(bp) > HDRP(bp));
      return;
  }
  
  // Can successfully split, allocate block of asize
  PUT(HDRP(bp), PACK(asize, 1));
  PUT(FTRP(bp), PACK(asize, 1)); 
  DBG_PRINT("allocated block at %p, size from header = %d, size from foote = %d\n", bp, GET_SIZE(HDRP(bp)), GET_SIZE(FTRP(bp)));
  DBG_ASSERT(FTRP(bp) > HDRP(bp));
  
  // Mark next block of size rem_size as empty and add to seg_list
  PUT(HDRP(NEXT_BLKP(bp)), PACK(rem_size, 0));
  PUT(FTRP(NEXT_BLKP(bp)), PACK(rem_size, 0));

  seg_list_add((list_block*)NEXT_BLKP(bp));
}

/**********************************************************
 * print_heap
 * Print the entire known heap.
 **********************************************************/
void print_heap(){
    void* it = prologue;
    for (it = prologue; it != epilogue; it = NEXT_BLKP(it)){
        printf("blk at %p, size=%lu, alloc=%d\n", it, GET_SIZE(HDRP(it)),(int) GET_ALLOC(HDRP(it)));
        if(GET_SIZE(HDRP(it)) == 0){
            printf("manual break point...\n");
        }
    }
    printf("epilogue at %p, size=%lu, alloc=%d\n", it, GET_SIZE(HDRP(it)),(int) GET_ALLOC(HDRP(it)));
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
    
    DBG_ASSERT(bp > start_of_heap);
    DBG_ASSERT(mm_check());
    DBG_PRINT("Free request for 0x%p size of %x\n", bp, GET_SIZE(HDRP(bp)));
    
    DBG_ASSERT(FTRP(bp) > HDRP(bp));
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    DBG_ASSERT(FTRP(bp) > HDRP(bp));

    seg_list_add(coalesce(bp));
    DBG_PRINT_HEAP();
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
        DBG_ASSERT((void*)bp > start_of_heap);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    DBG_ASSERT((void*)bp > start_of_heap);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{   
    int orig_sz = GET_SIZE(HDRP(ptr));
    DBG_ASSERT(GET(HDRP(ptr)) == GET(FTRP(ptr)));


    DBG_PRINT_HEAP();

    DBG_PRINT("realloc request for 0x%p orig_sz: %x, request_size: %x\n", ptr, orig_sz, size);
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    DBG_ASSERT(FTRP(ptr) > HDRP(ptr));

    size_t asize; /* adjusted block size */
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);


    DBG_PRINT("heap epilogue now at:%p\n", epilogue);
    if (asize == orig_sz){
        DBG_ASSERT(FTRP(ptr) > HDRP(ptr));
        return ptr;
    }else if (asize < orig_sz){
        int rem_size = orig_sz - asize;
        if(rem_size < (2 * DSIZE)){
            //couldn't split
            return ptr;
        }
        // Can successfully split, allocate block of asize
        place(ptr,asize);
        return ptr;
    }else{ // size > ptr
 	DBG_ASSERT(GET(HDRP(ptr)) == GET(FTRP(ptr)));
        int i_size;
        int i = 0;
        void *iptr = ptr;
        // loop will never go beyond 2 iterations
        for (i_size = 0; i_size < asize; iptr = NEXT_BLKP(iptr)) {
            if ((GET_ALLOC(HDRP(iptr)) && i != 0) || iptr > epilogue)   {
                DBG_PRINT("CONTIGUOUS ALLOCATION FAILED!!\n");
                // failed to find contigous memory block, allocate new block using malloc
                void* newptr = mm_malloc(asize);
                memcpy(newptr, ptr, orig_sz);

                DBG_ASSERT(FTRP(ptr) > HDRP(ptr));
                mm_free(ptr);
                DBG_ASSERT(FTRP(newptr) > HDRP(newptr));
                DBG_PRINT_HEAP();
                return newptr;
            }
        DBG_ASSERT(FTRP(iptr) > HDRP(iptr));
        DBG_PRINT("%dth block @ %p, size: %x\n", i, iptr, GET_SIZE(HDRP(iptr)));
	    DBG_ASSERT(GET(HDRP(iptr)) == GET(FTRP(iptr)));
            i_size += GET_SIZE(HDRP(iptr));
            i++;
        }
        // success, mark all i blocks as allocated, remove from free lists, return same ptr
        DBG_PRINT("CONTIGUOUS BlOCKS SUCCESS! original block at:%p, i_size:%x, asize:%x, orig_sz:%x, num_blocks:%d\n", ptr, i_size, asize, orig_sz, i-1);
        int ii;

        //loop won't go beyond 1 iteration
        for (iptr = NEXT_BLKP(ptr), ii=1; ii < i; ii++, iptr = NEXT_BLKP(iptr)){
            DBG_PRINT("removing %dth block @ 0x%p, size: %x\n", ii, iptr, GET_SIZE(HDRP(iptr)));
            seg_list_remove((list_block*)iptr);
        }

        //can add remainder to seg list but don't to optimize for realloc heavy lab
        DBG_ASSERT(i_size >= asize);
        PUT(HDRP(ptr), PACK(i_size, 1));
        PUT(FTRP(ptr), PACK(i_size, 1));
        DBG_ASSERT(FTRP(ptr) > HDRP(ptr));
    	DBG_PRINT("allocated block at %p, size from header = %x, size from footer = %x\n", ptr, GET_SIZE(HDRP(ptr)), GET_SIZE(FTRP(ptr)));
        DBG_PRINT_HEAP();
        return ptr;
    }

}

/**********************************************************
 * count_in_free_list
 * Count the number of times a free block occurs in
 * the free lists. 
 *********************************************************/
int count_in_free_list(void* p){
    int i, count=0;
    for (i = 0; i < NUM_LISTS; i++) {
        list_block *ls = seg_lists[i];
        if (!ls) continue;
        do {
            if (p==ls){
                count++;
            }
            ls = ls->next;
        } while (ls != seg_lists[i]); 
    }
    return count;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
    void* it = prologue;
    for (it = prologue; it < epilogue; it = NEXT_BLKP(it)){
        // no block is of size 0
        if(GET_SIZE(HDRP(it)) == 0){
            return 0;
        }
        // headers and footers match
        if(GET(HDRP(it)) != GET(HDRP(it))){
            return 0;
        }
        
        if(!GET_ALLOC(HDRP(it))){
            // is every free block in the free list (and occurs once only)?
            if(count_in_free_list(it) != 1){
                return 0;
            }
            // are there any free blocks that escaped coalescing?
            if(!GET_ALLOC(HDRP(NEXT_BLKP(it)))){
                return 0;
            }
        }

        // is there any overlapping?
        if(FTRP(it) > HDRP(NEXT_BLKP(it))){
            return 0;
        }
    }
    // last block is always epilogue
    if (it != epilogue){
        return 0;
    }

    // is every block in the free list marked as free?
    int i;
    for (i = 0; i < NUM_LISTS; i++) {
        list_block *ls = seg_lists[i];
        if (!ls) continue;
        do {
            if(GET_ALLOC(HDRP(ls))){
                return 0;
            }
            ls = ls->next;
        } while (ls != seg_lists[i]); 
    }
    return 1;
}

