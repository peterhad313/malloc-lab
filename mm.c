/* Basic malloc with an explicit free list */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "memlib.h"
#include "mm.h"
/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

team_t team = {
    /* Team name */
    "JacobAndPeter",
    /* First member's full name */
    "Jacob Kobza",
    /* First member's email address */
    "jacobkobza2016@u.northwestern.edu",
    /* Second member's full name (leave blank if none) */
    "Peter Haddad",
    /* Second member's email address (leave blank if none) */
    "peterhaddad2016@u.northwestenrn.edu"
};
// Basic constants and macros
#define WSIZE      4 /* Word and header/footer size (bytes) */
#define DSIZE      8    /* Doubleword size (bytes) */
#define CHUNKSIZE  (1 << 12)      /* Extend heap by this amount (bytes) */

/*Return the larger of x and y*/
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p. */
#define GET(p)       (*(uintptr_t *)(p))
#define PUT(p, val)  (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)   (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)  (GET(p) & 0x1)


/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)  ((void *)(bp) - WSIZE)
#define FTRP(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLK(bp)  ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLK(bp)  ((void *)(bp) - GET_SIZE((void *)(bp) - DSIZE))

// Get adjacent free pointer
#define GET_NEXT_FREE(bp)  (*(char **)(bp + WSIZE))
#define GET_PREV_FREE(bp)  (*(char **)(bp))

//Place adjacent free pointers
#define SET_NEXT_FREE(bp, qp) (GET_NEXT_FREE(bp) = qp)
#define SET_PREV_FREE(bp, qp) (GET_PREV_FREE(bp) = qp)

/* Global declarations */
static char *heap_listp = 0; 
static char *free_listp = 0;

/* Function prototypes for internal helper routines */
static void *coalesce(void *bp);
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* Free List Functions*/
static void addToFree(void *bp); 
static void removeFromFree(void *bp); 

/* Heap Check FUnctions */
static void checkblock(void *bp);
static void checkheap(int verbose);
static void printblock(void *bp); 

/**
 * mm_init - Initialize the memory manager.
 */
int mm_init(void) {
  
  /* Create the initial empty heap. */
  if ((heap_listp = mem_sbrk(8*WSIZE)) == NULL){
//     printf("ERROR");
    return -1;
  }

  PUT(heap_listp, 0);                            /* Alignment padding */
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
  free_listp = heap_listp + 2*WSIZE; 

  /* Extend the empty heap with a free block of minimum possible block size */
  if (extend_heap(CHUNKSIZE/WSIZE) == NULL){ 
    return -1;
  }
  return 0;
}

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
void *mm_malloc(size_t size) 
{
  size_t asize;      /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  char *bp;

  /* Ignore spurious requests. */
  if (size == 0)
    return (NULL);

  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE)
    asize = 2 * DSIZE;
  else
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

  /* Search the free list for a fit. */
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return (bp);
  }

  /* No fit found.  Get more memory and place the block. */
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)  
    return (NULL);
  place(bp, asize);
  return bp;
} 

/* 
 * mm_free - Free a block 
 */
void mm_free(void *bp){

  /* Ignore incorrect requests. */
  if (bp == NULL)
    return;
 
  size_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

/*
 * mm_realloc - Naive implementation of realloc
 */ 
void *mm_realloc(void *ptr, size_t size){
  
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
} 

/* Loop through the heap anc check if all blocks are valid */
// void checkheap(int verbose) 
// {
//   char *bp;
// 
//   if (verbose)
//     printf("Start of heap: (%p)\n", heap_listp);
// 
//   for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = (char *)NEXT_BLK(bp)) {
//     if (verbose)
//       printblock(bp);
//     checkblock(bp);
//   }
// 
// }


/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */

static void *extend_heap(size_t words) {
  char *bp;
  size_t size;

  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

  if ((long)(bp = mem_sbrk(size)) == -1){ 
    return NULL;
  }
  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0));         /* free block header */
  PUT(FTRP(bp), PACK(size, 0));         /* free block footer */
  PUT(HDRP(NEXT_BLK(bp)), PACK(0, 1)); /* new epilogue header */
  /* Coalesce if the previous block was free */
  return coalesce(bp);
}

/* Puts block of size asize at loc bp */
void place(void *bp, size_t asize){
  size_t csize = GET_SIZE(HDRP(bp));

  if ((csize - asize) >= 2 * DSIZE) {
    //make new block
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    //remove free from list
    removeFromFree(bp);
    //create new block and add it to the list
    bp = NEXT_BLK(bp);
    PUT(HDRP(bp), PACK(csize-asize, 0));
    PUT(FTRP(bp), PACK(csize-asize, 0));
    coalesce(bp);
  } else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
    removeFromFree(bp);
  }
}


/* find a free block of asize bytes */

static void *find_fit(size_t asize){
  void *bp;

  for (bp = free_listp; GET_ALLOC(HDRP(bp)) == 0; bp = GET_NEXT_FREE(bp) ){
    if (asize <= (size_t)GET_SIZE(HDRP(bp)) ) {
      return bp;
    }
    
  }
  return NULL;
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp){

  size_t NEXT_ALLOC = GET_ALLOC(  HDRP(NEXT_BLK(bp))  );
  size_t PREV_ALLOC = GET_ALLOC(  FTRP(PREV_BLK(bp))  ) || PREV_BLK(bp) == bp;
  size_t size = GET_SIZE(HDRP(bp));
  
  /* Next block is free */   
  if (PREV_ALLOC && !NEXT_ALLOC) {                  
    size += GET_SIZE( HDRP(NEXT_BLK(bp))  );
    removeFromFree(NEXT_BLK(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }
  /* Prev block is free */  
  else if (!PREV_ALLOC && NEXT_ALLOC) {               
    size += GET_SIZE( HDRP(PREV_BLK(bp))  );
    bp = PREV_BLK(bp);
    removeFromFree(bp);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }
  /* Both blocks free */ 
  else if (!PREV_ALLOC && !NEXT_ALLOC) {                
    size += GET_SIZE( HDRP(PREV_BLK(bp))  ) + GET_SIZE( HDRP(NEXT_BLK(bp))  );
    removeFromFree(PREV_BLK(bp));
    removeFromFree(NEXT_BLK(bp));
    bp = PREV_BLK(bp);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
  }/* lastly insert bp into free list and return bp */
  addToFree(bp);
  return bp;
}

/*Inserts the free block pointer int the free_list*/
static void addToFree(void *bp){
  SET_NEXT_FREE(bp, free_listp); 
  SET_PREV_FREE(free_listp, bp); 
  SET_PREV_FREE(bp, NULL); 
  free_listp = bp; 
}
/*Removes the free block pointer int the free_list*/
static void removeFromFree(void *bp){
  if (GET_PREV_FREE(bp))
    SET_NEXT_FREE(GET_PREV_FREE(bp), GET_NEXT_FREE(bp));
  else
    free_listp = GET_NEXT_FREE(bp);
  SET_PREV_FREE(GET_NEXT_FREE(bp), GET_PREV_FREE(bp));
}
// 
// 
// /* 
//  * The remaining routines are heap consistency checker routines. 
//  */
// 
// /*
//  * Requires:
//  *   "bp" is the address of a block.
//  *
//  * Effects:
//  *   Perform a minimal check on the block "bp".
//  */
// static void
// checkblock(void *bp) 
// {
// 
//   if ((uintptr_t)bp % DSIZE)
//     printf("Error: %p is not doubleword aligned\n", bp);
//   if (GET(HDRP(bp)) != GET(FTRP(bp)))
//     printf("Error: header does not match footer\n");
// }
// 
// 
// 
// /*
//  * Requires:
//  *   "bp" is the address of a block.
//  *
//  * Effects:
//  *   Print the block "bp".
//  */
// static void
// printblock(void *bp) 
// {
//   bool halloc, falloc;
//   size_t hsize, fsize;
// 
//   checkheap(false);
//   hsize = GET_SIZE(HDRP(bp));
//   halloc = GET_ALLOC(HDRP(bp));  
//   fsize = GET_SIZE(FTRP(bp));
//   falloc = GET_ALLOC(FTRP(bp));  
// 
//   if (hsize == 0) {
//     printf("%p: end of heap\n", bp);
//     return;
//   }
// 
//   printf("%p: header: [%zu:%c] footer: [%zu:%c]\n", bp, 
//       hsize, (halloc ? 'a' : 'f'), 
//       fsize, (falloc ? 'a' : 'f'));
// }
