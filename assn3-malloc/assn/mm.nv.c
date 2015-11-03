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
    "Nayeem Husain Zen",
    /* First member's email address */
    "nayeem.zen@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Zaid Al Khishman",
    /* Second member's email address (leave blank if none) */
    "zaid.al.khishman@mail.utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
 *************************************************************************/
//#define DEBUG
//#define VERBOSE

#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))
#define MIN(x,y) ((x) < (y)?(x) :(y))

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

/* Given free block ptr bp, compute address to next/prev free blocks */
#define NEXT_FREE_BLKP_PTR(bp) ((char *)(bp) + WSIZE)
#define PREV_FREE_BLKP_PTR(bp) ((char *)(bp) )
#define NEXT_FREE_BLKP(bp) (GET(NEXT_FREE_BLKP_PTR(bp)))
#define PREV_FREE_BLKP(bp) (GET(PREV_FREE_BLKP_PTR(bp)))

#ifdef DEBUG
    #define DBG_PRINTF(...) printf(__VA_ARGS__)
    #define DBG_ASSERT(x)   assert(x)
#else
    #define DBG_PRINTF(...)
    #define DBG_ASSERT(x)
#endif

int mm_check(void);
static void printblock(void *);
void *heap_listp = NULL;

/**********************************************************
 * Variables for Segregated Lists
 **********************************************************/
#define SL_SIZE 14
#define SL_SMALLEST_BUCKET_FLOOR 32
#define SL_BUCKET_CEILING(floor) ((floor << 1) - 1)
static void *sl[SL_SIZE];

/**********************************************************
 * Function Prototypes for Segregated Lists
 **********************************************************/
void sl_init(void) __attribute__((always_inline));
int sl_bsize_to_bucket_index(size_t bsize) __attribute__((always_inline));
void sl_print(void) __attribute__((always_inline));
int sl_exists(void *bp) __attribute__((always_inline));
void sl_insert(void *bp) __attribute__((always_inline));
void sl_remove(void *bp) __attribute__((always_inline));
void * sl_split(void *bp, size_t asize) __attribute__((always_inline));
void sl_crop(void *bp, size_t asize) __attribute__((always_inline));
void * sl_find_fit(size_t asize) __attribute__((always_inline));
void * sl_split_remove_place(void* bp, size_t asize) __attribute__((always_inline));

/**********************************************************
 * sl_init
 * Initialize the segregated list to NULL values
 **********************************************************/
void sl_init(void) {
  DBG_PRINTF("Entering sl_init\n");
  int i;
  for(i = 0; i < SL_SIZE; i++) {
    sl[i] = NULL;
  }
}

/**********************************************************
 * sl_bsize_to_bucket_index
 * Given a blk size, find the correct bucket index in the sl
 **********************************************************/
int sl_bsize_to_bucket_index(size_t bsize) {
  DBG_ASSERT(bsize >= (2 * DSIZE));
  DBG_ASSERT(bsize % 16 == 0);
  size_t bucket_ceiling = SL_BUCKET_CEILING(SL_SMALLEST_BUCKET_FLOOR);
  int index = 0;
  while (bsize > bucket_ceiling) {
    index++;
    if (index == SL_SIZE - 1) return index; // Max index reached
    bucket_ceiling = SL_BUCKET_CEILING((bucket_ceiling + 1));
  }
  DBG_ASSERT(index < SL_SIZE);
  return index;
}

/**********************************************************
 * sl_print
 * Print contents of the segregated list for debugging
 **********************************************************/
void sl_print(void) {
  int i;
  DBG_PRINTF("***** Segregated List *****\n");
  size_t bucket_floor = SL_SMALLEST_BUCKET_FLOOR;
  for(i = 0; i < SL_SIZE; i++) {
    void *ptr = sl[i];
    if (ptr) {
      DBG_PRINTF(
        "***** sizes %zu to %zu *****\n",
        bucket_floor,
        SL_BUCKET_CEILING(bucket_floor)
      );
      while(ptr) {
        printblock(ptr);
        ptr = (void *) NEXT_FREE_BLKP(ptr);
      }
    }
    bucket_floor <<= 1;
  }
  DBG_PRINTF("***** End Segregated List *****\n");
}

/**********************************************************
 * sl_exists
 * Function to see if a block is in sl. This function is
 * purely for debugging purposes and shouldn't really
 * be used unless needed.
 **********************************************************/
int sl_exists(void *bp) {
  DBG_ASSERT(bp);
  DBG_ASSERT(!GET_ALLOC(HDRP(bp)));
  int i;
  for(i = 0; i < SL_SIZE; i++) {
    void *head = sl[i];
    while (head) {
      if (head == bp) return 1;
      head = (void *) NEXT_FREE_BLKP(head);
    }
  }
  return 0;
}

/**********************************************************
 * sl_insert (sl_insert_in_size_order)
 * Insert a free block into the segregated list based on
 * its size. Each bucket is a doubly linked list, and we
 * insert a blk based on a largest first policy.
 **********************************************************/
void sl_insert(void *bp) {
  DBG_PRINTF("Entering sl_insert_in_size_order\n");
  DBG_ASSERT(bp);
#ifdef DEBUG
  printblock(bp);
#endif
  DBG_ASSERT(!sl_exists(bp));
  DBG_ASSERT(!GET_ALLOC(HDRP(bp)));
  DBG_ASSERT(!GET_ALLOC(FTRP(bp)));

  size_t bsize = GET_SIZE(HDRP(bp));
  DBG_ASSERT(bsize % 16 == 0);

  // Find right bucket
  int bucket_index = sl_bsize_to_bucket_index(bsize);

  // Insert bp based on size, biggest at the front
  void *cur = sl[bucket_index];

  // Check for empty list
  if (!cur) {
    sl[bucket_index] = bp;
    PUT(NEXT_FREE_BLKP_PTR(bp), 0);
    PUT(PREV_FREE_BLKP_PTR(bp), 0);
#ifdef DEBUG
    mm_check();
#endif
    DBG_PRINTF("Exiting sl_insert_in_size_order\n");
    return;
  }

  void *prev = NULL;
  while (cur) {
    size_t cur_size = GET_SIZE(HDRP(cur));
    if (bsize >= cur_size) break;
    prev = cur;
    cur = (void *) NEXT_FREE_BLKP(cur);
  }

  // We want to put bp in between prev and cur
  // First adjust pointers of bp
  PUT(NEXT_FREE_BLKP_PTR(bp), (uintptr_t) cur);
  PUT(PREV_FREE_BLKP_PTR(bp), (uintptr_t) prev);

  // Now adjust prev and cur so they point to bp
  if (prev) {
    PUT(NEXT_FREE_BLKP_PTR(prev), (uintptr_t) bp);
  } else {
    // Head of the list needs to be updated if prev is NULL
    sl[bucket_index] = bp;
  }
  if (cur) PUT(PREV_FREE_BLKP_PTR(cur), (uintptr_t) bp);

#ifdef DEBUG
  mm_check();
#endif
  DBG_PRINTF("Exiting sl_insert_in_size_order\n");
}

/**********************************************************
 * sl_remove
 * Remove a free block from the segregated list. We assume
 * that the free block exists in the segregated list.
 **********************************************************/
void sl_remove(void *bp) {
  DBG_PRINTF("Entering sl_remove\n");
  DBG_ASSERT(bp);
#ifdef DEBUG
  printblock(bp);
#endif
  // Check if bp is not null and a free block
  DBG_ASSERT(!GET_ALLOC(HDRP(bp)));
  DBG_ASSERT(sl_exists(bp));

  // Figure out which bucket the block is in
  size_t bsize = GET_SIZE(HDRP(bp));
  int bucket_index = sl_bsize_to_bucket_index(bsize);

  // Corner case when we're removing the head
  if (sl[bucket_index] == bp) {
    sl[bucket_index] = (void *) NEXT_FREE_BLKP(bp);
  }

  // Remove from bucket (doubly linked list)
  void *prev = (void *) PREV_FREE_BLKP(bp);
  if(prev) {
    PUT(NEXT_FREE_BLKP_PTR(prev), NEXT_FREE_BLKP(bp));
  }
  void *next = (void *) NEXT_FREE_BLKP(bp);
  if(next) {
    PUT(PREV_FREE_BLKP_PTR(next), PREV_FREE_BLKP(bp));
  }

#ifdef DEBUG
  mm_check();
#endif
  DBG_PRINTF("Exiting sl_remove\n");
}

/**********************************************************
 * sl_split
 * Split a free block if possible and ideally returns a blk
 * with perfect size for asize. If split, we put the two
 * smaller blocks back into the sl.
 * We determine how to split it based on the sizes of the
 * adjacent blocks and asize. In general, we want to keep
 * big allocated chunks of memory together and smaller chunks
 * of memory together so we can avoid external fragmentation.
 * Returns a block ptr for asize
 **********************************************************/
void * sl_split(void *bp, size_t asize) {
  DBG_PRINTF("Entering split\n");
#ifdef DEBUG
  printblock(bp);
#endif
  DBG_ASSERT(bp);
    DBG_ASSERT(!GET_ALLOC(HDRP(bp)));
  DBG_ASSERT(asize >= (2 * DSIZE));
  DBG_ASSERT(asize % 16 == 0);

  size_t bsize = GET_SIZE(HDRP(bp));
  DBG_ASSERT(bsize >= asize);

  // Calculate the remaining size of the block if we were to split it for asize
  size_t remaining_bsize = bsize - asize;

  // Check if can split or if the remaining block would be too small
  if (remaining_bsize < (2 * DSIZE)) {
    DBG_PRINTF("Couldn't split\n");
    return bp;
  }

    // Figure out where to place asize depending on its size, the prev blk size,
  // and the next blk size. We want to group big blocks with each other and
  // small blocks with each other to avoid external fragmentation.
    size_t prev_bsize = GET_SIZE(FTRP(PREV_BLKP(bp)));
  size_t next_bsize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
    size_t biggest_adj_bsize = MAX(prev_bsize, next_bsize);

    size_t avg_size = (prev_bsize + next_bsize) / 2;
  /*  if (GET_SIZE(HDRP(bp)) > prev_bsize) {
            // Move it beside to prev (the bigger adj blk)
            sl_remove(bp);
            PUT(HDRP(bp), PACK(asize, 0));
            PUT(FTRP(bp), PACK(asize, 0));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(remaining_bsize, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(remaining_bsize, 0));
            sl_insert(bp);
            sl_insert(NEXT_BLKP(bp));
            return bp;
        } else {
            // Move it beside to next (the bigger adj blk)
            sl_remove(bp);
            PUT(HDRP(bp), PACK(remaining_bsize, 0));
            PUT(FTRP(bp), PACK(remaining_bsize, 0));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 0));
            sl_insert(bp);
            sl_insert(NEXT_BLKP(bp));
            return NEXT_BLKP(bp);
        }*/
    if (asize >= avg_size) {
    // asize is bigger than the avg size of the prev and next blk, so allocate
    // it towards the bigger adjacent block

        if (biggest_adj_bsize == prev_bsize) {
            // Move it beside to prev (the bigger adj blk)
            sl_remove(bp);
            PUT(HDRP(bp), PACK(asize, 0));
            PUT(FTRP(bp), PACK(asize, 0));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(remaining_bsize, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(remaining_bsize, 0));
            sl_insert(bp);
            sl_insert(NEXT_BLKP(bp));
            return bp;
        } else {
            // Move it beside to next (the bigger adj blk)
            sl_remove(bp);
            PUT(HDRP(bp), PACK(remaining_bsize, 0));
            PUT(FTRP(bp), PACK(remaining_bsize, 0));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 0));
            sl_insert(bp);
            sl_insert(NEXT_BLKP(bp));
            return NEXT_BLKP(bp);
        }

    } else {
    // asize is smaller than avg size, so move it beside the smaller adj blk

        if (biggest_adj_bsize == next_bsize) {
            // Move it beside to prev (the smaller adj blk)
            sl_remove(bp);
            PUT(HDRP(bp), PACK(asize, 0));
            PUT(FTRP(bp), PACK(asize, 0));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(remaining_bsize, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(remaining_bsize, 0));
            sl_insert(bp);
            sl_insert(NEXT_BLKP(bp));
            return bp;
        } else {
            // Move it beside to next (the smaller adj blk)
            sl_remove(bp);
            PUT(HDRP(bp), PACK(remaining_bsize, 0));
            PUT(FTRP(bp), PACK(remaining_bsize, 0));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 0));
            PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 0));
            sl_insert(bp);
            sl_insert(NEXT_BLKP(bp));
            return NEXT_BLKP(bp);
        }
    } 

#ifdef DEBUG
  DBG_PRINTF("Split into:\n");
  printblock(bp);
  printblock(NEXT_BLKP(bp));
  mm_check();
#endif
}

/**********************************************************
 * sl_crop
 * Crop a block of memory to a certain size and then mark
 * the scraps as free and put it back int sl
 **********************************************************/
void sl_crop(void *bp, size_t asize) {
  DBG_PRINTF("Entering sl_crop for %p\n", bp);
#ifdef DEBUG
  printblock(bp);
#endif
  DBG_ASSERT(bp);
    DBG_ASSERT(GET_ALLOC(HDRP(bp)));
  DBG_ASSERT(asize >= (2 * DSIZE));
  DBG_ASSERT(asize % 16 == 0);

  size_t bsize = GET_SIZE(HDRP(bp));
  DBG_ASSERT(bsize >= asize);

  // We can't crop it because the remaining block would be too small
  size_t remaining_bsize = bsize - asize;
  if (remaining_bsize < (2 * DSIZE)) {
    DBG_PRINTF("Couldn't crop\n");
    return;
  }

  // Crop
  PUT(HDRP(bp), PACK(asize, 1));
  PUT(FTRP(bp), PACK(asize, 1));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(remaining_bsize, 0));
  PUT(FTRP(NEXT_BLKP(bp)), PACK(remaining_bsize, 0));

  // Put the scraps in the free list
  sl_insert(NEXT_BLKP(bp));

#ifdef DEBUG
  DBG_PRINTF("Cropped into:\n");
  printblock(bp);
  printblock(NEXT_BLKP(bp));
  mm_check();
#endif
}

/**********************************************************
 * sl_find_fit
 * Traverse the sl searching for a block to fit asize
 * We start with the bucket that asize is supposed to be in
 * and keep going up until we find something (or not)
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * sl_find_fit(size_t asize) {
  DBG_PRINTF("Entering sl_find_fit for %zu\n", asize);
#ifdef DEBUG
  mm_check();
#endif
  DBG_ASSERT(asize >= (2 * DSIZE));
  DBG_ASSERT(asize % 16 == 0);

  // Go through the sl and look for a fit
  int i;
  void *bp = NULL;
  for (i = sl_bsize_to_bucket_index(asize); i < SL_SIZE; i++) {
    // Just see if the head is big enough to fit us
    void *head = sl[i];
    if (head && GET_SIZE(HDRP(head)) >= asize) {
      bp = head;
      break;
    }
  }
  // Will return NULL if nothing found
  return bp;
}

/**********************************************************
 * sl_split_remove_place
 * Basically this function places a blk (marks it as allocated)
 * We do the following:
 * - Try to split a free block
 * - Remove a free block to mark
 * - Return a pointer to the new bp that's marked
 **********************************************************/
void * sl_split_remove_place(void* bp, size_t asize) {
  DBG_PRINTF("Entering sl_split_remove_place for %p\n",bp);
#ifdef DEBUG
  printblock(bp);
  mm_check();
#endif

  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));
  DBG_ASSERT(bsize >= asize);

  // Split and get the (possibly new) size
  bp = sl_split(bp, asize);
  bsize = GET_SIZE(HDRP(bp));

  // Remove it from sl and allocate
  sl_remove(bp);
  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));

#ifdef DEBUG
  mm_check();
#endif
  DBG_PRINTF("Exiting sl_split_remove_place\n");

  return bp;
}

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void)
{
    DBG_PRINTF("Entering mm_init\n");
  if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
    return -1;
  PUT(heap_listp, 0);                         // alignment padding
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));   // prologue header
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));   // prologue footer
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    // epilogue header
  heap_listp += DSIZE;

  // Initialize segregated list
  sl_init();

  return 0;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 * Coalesce also manipulates blks in the sl so that they
 * go into the correct bucket
 **********************************************************/
void *coalesce(void *bp)
{
    DBG_PRINTF("Entering coalesce\n");
  DBG_ASSERT(!GET_ALLOC(HDRP(bp)));
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {       /* Case 1 */
    sl_insert(bp);
    return bp;
  }

  else if (prev_alloc && !next_alloc) { /* Case 2 */
    DBG_PRINTF("About to remove %p from freelist\n", NEXT_BLKP(bp));
    sl_remove(NEXT_BLKP(bp));

    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    sl_insert(bp);
    return (bp);
  }

  else if (!prev_alloc && next_alloc) { /* Case 3 */
    DBG_PRINTF("About to remove %p from freelist\n", PREV_BLKP(bp));
    sl_remove(PREV_BLKP(bp));

    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

    sl_insert(PREV_BLKP(bp));
    return (PREV_BLKP(bp));
  }

  else {            /* Case 4 */
    DBG_PRINTF(
      "About to remove %p and %p from freelist\n",
      PREV_BLKP(bp),
      NEXT_BLKP(bp)
    );
    sl_remove(PREV_BLKP(bp));
    sl_remove(NEXT_BLKP(bp));

    size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
      GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
    PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));

    sl_insert(PREV_BLKP(bp));
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
    DBG_PRINTF("Entering extend_heap by %zu\n", words * WSIZE);
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
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    DBG_PRINTF("Entering mm_free to free ptr %p\n",bp);
  if(bp == NULL){
    return;
  }
  size_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size,0));
  PUT(FTRP(bp), PACK(size,0));
  coalesce(bp);
#ifdef DEBUG
    mm_check();
#endif
}

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by sl_find_fit
 * The decision of splitting the block, or not is determined
 * by sl_split_remove_place.
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    DBG_PRINTF("Entering mm_malloc to allocate %zd bytes\n", size);
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
  DBG_PRINTF("Adjusted asize is %zu\n", asize);

  /* Search sl for a fit */
  if ((bp = sl_find_fit(asize)) != NULL) {
    bp = sl_split_remove_place(bp, asize);
    return bp;
  }

  /* No fit found. Get more memory and place the block */
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;
  bp = sl_split_remove_place(bp, asize);
  DBG_PRINTF("list extended\n");
#ifdef DEBUG
    mm_check();
#endif

  return bp;
}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 * We look at adjacent memory blocks and see if they're free.
 * If they are and the size that's required fits, we don't
 * have to allocate new memory. Instead, we do a memmove if
 * required (move to start at previous block).
 * The logic in this function is very similar to coalesce.
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    DBG_PRINTF("Entering mm_realloc for %p, new size %zu\n", ptr, size);
  /* If size == 0 then this is just free, and we return NULL. */
  if(size == 0){
    mm_free(ptr);
    return NULL;
  }
  /* If oldptr is NULL, then this is just malloc. */
  if (ptr == NULL)
    return (mm_malloc(size));

  // Using similar logic as coalesce (4 cases)
  // Look at prev alloc and next alloc and see if they're readily available
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(ptr)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
  size_t readily_available_size = GET_SIZE(HDRP(ptr));

  // Figure out the copy size
  void *oldptr = ptr;
  void *newptr;
  size_t copySize = GET_SIZE(HDRP(ptr));
  if (size < copySize) copySize = size;

  size_t asize; /* adjusted block size */
  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE)
    asize = 2 * DSIZE;
  else
    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

  if (readily_available_size >= asize) {
    // We fit! Try to crop out any extra memory if possible
    sl_crop(ptr, asize);
    return ptr;
  }

  if (prev_alloc && next_alloc) {
    // There aren't any free neighbours beside us.

  } else if (prev_alloc && !next_alloc) {
    // Next blk is free
    readily_available_size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    if (readily_available_size >= asize) {
      // We fit! Remove the next block from sl
      sl_remove(NEXT_BLKP(ptr));

      // Modify sizes
      PUT(HDRP(ptr), PACK(readily_available_size, 1));
      PUT(FTRP(ptr), PACK(readily_available_size, 1));

      // Crop out anything extra
      sl_crop(ptr, asize);
      return ptr;
    }

  } else if (!prev_alloc && next_alloc) {
    // Prev blk is free
    readily_available_size += GET_SIZE(HDRP(PREV_BLKP(ptr)));

    if (readily_available_size >= asize) {
      // We fit! Remove the prev block from sl
      sl_remove(PREV_BLKP(ptr));

      // Modify sizes
      PUT(FTRP(ptr), PACK(readily_available_size, 1));
      PUT(HDRP(PREV_BLKP(ptr)), PACK(readily_available_size, 1));

      // Move over stuff to start at prev blk
      newptr = PREV_BLKP(ptr);
      memmove(newptr, oldptr, copySize);

      // Crop out anything extra
      sl_crop(newptr, asize);
      return newptr;
    }

  } else {
    // Prev and next both free
    // Check if next block is enough so we don't have to memmove
    readily_available_size += GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    if (readily_available_size >= asize) {
      // we fit. Remove next from sl
      sl_remove(NEXT_BLKP(ptr));

      // Modify sizes
      PUT(HDRP(ptr), PACK(readily_available_size, 1));
      PUT(FTRP(ptr), PACK(readily_available_size, 1));

      // Crop out anything extra
      sl_crop(ptr, asize);
      return ptr;
    }

    readily_available_size += GET_SIZE(HDRP(PREV_BLKP(ptr)));

    if (readily_available_size >= asize) {
      // We fit. Remove next and prev from sl
      sl_remove(NEXT_BLKP(ptr));
      sl_remove(PREV_BLKP(ptr));

      // Modify sizes
      PUT(HDRP(PREV_BLKP(ptr)), PACK(readily_available_size, 1));
      PUT(FTRP(NEXT_BLKP(ptr)), PACK(readily_available_size, 1));

      // Move over stuff to start at prev blk
      newptr = PREV_BLKP(ptr);
      memmove(newptr, oldptr, copySize);

      // Crop out anything extra
      sl_crop(newptr, asize);
      return newptr;
    }
  }

  // Nothing readily available.
  DBG_PRINTF("realloc old fashioned way\n");
  newptr = mm_malloc(size);
  if (newptr == NULL)
    return NULL;
  memcpy(newptr, oldptr, copySize);
  mm_free(oldptr);
  return newptr;
}

static void printblock(void *bp)
{
  size_t hsize, halloc, fsize, falloc;
  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));

    // this is the epilogue block
  if (hsize == 0) {
    printf("%p: End of heap\n", bp);
    return;
  }

  printf("%p: header tag: [%zu:%s] footer tag: [%zu:%s]\n", bp,
      hsize, (halloc ? "allocated" : "free"),
      fsize, (falloc ? "allocated" : "free"));
}

static void checkblock(void *bp)
{
  if ((size_t)bp % 8)
    printf("ERROR: %p is not doubleword aligned\n", bp);
  if (GET(HDRP(bp)) != GET(FTRP(bp)))
    printf("ERROR: header does not match footer for %p\n",bp);
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
  DBG_PRINTF("\n===== Entering mm_check =====\n");
  char *bp = heap_listp;
#ifdef VERBOSE
  printf("Starting Heap Address (%p)\n", heap_listp);
#endif
  if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
    printf("ERROR: faulty prologue header\n");
  checkblock(heap_listp);

    // print all blocks in the heap
  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
#ifdef VERBOSE
    printblock(bp);
#endif
    checkblock(bp);
  }

#ifdef VERBOSE
    // print one more time for the epilogue block
  printblock(bp);
#endif
  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
    printf("ERROR: faulty epilogue header\n");

#ifdef VERBOSE
  // Print contents of the segregated list
  sl_print();
#endif
    DBG_PRINTF("===== Leaving mm_check =====\n\n");
  return 0;
}