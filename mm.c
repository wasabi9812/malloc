/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team6",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

// 함수 원형 선언
static void *extend_heap(size_t words);





// Basic constants and macros
#define WSIZE 4 // 워드 = 헤더 = 풋터 사이즈(bytes)
#define DSIZE 8 // 더블워드 사이즈(bytes)
#define CHUNKSIZE (1<<12) // heap을 이정도 늘린다(bytes)

#define MAX(x, y) ((x) > (y)? (x):(y))

// pack a size and allocated bit into a word 
#define PACK(size, alloc) ((size) | (alloc))

// Read and wirte a word at address p
//p는 (void*)포인터이며, 이것은 직접 역참조할 수 없다.
#define GET(p)     (*(unsigned int *)(p)) //p가 가리키는 놈의 값을 가져온다
#define PUT(p,val) (*(unsigned int *)(p) = (val)) //p가 가리키는 포인터에 val을 넣는다

// Read the size and allocated fields from address p 
#define GET_SIZE(p)  (GET(p) & ~0x7) // ~0x00000111 -> 0x11111000(얘와 and연산하면 size나옴)
#define GET_ALLOC(p) (GET(p) & 0x1)

// Given block ptr bp, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //헤더+데이터+풋터 -(헤더+풋터)

// Given block ptr bp, compute address of next and previous blocks
// 현재 bp에서 WSIZE를 빼서 header를 가리키게 하고, header에서 get size를 한다.
// 그럼 현재 블록 크기를 return하고(헤더+데이터+풋터), 그걸 현재 bp에 더하면 next_bp나옴
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))





/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */

static char *heap_listp;
int mm_init(void)
{
//Create the initial empty heap
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) 
        return -1;                              // 불러올 수 없으면 -1 return  
    PUT(heap_listp, 0);                         // Alignment padding
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); // P.H 8(크기)/1(할당됨)
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); // P.F 8/1
    PUT(heap_listp + (3*WSIZE), PACK(0,1));     // E.H(헤더로만 구성) 0/1
    heap_listp += (2*WSIZE); // 처음에 항상 prolouge 사이를 가리킴
    // 나중에 find_fit 함수에서 find할 때 사용됨
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) //word가 몇개인지 확인해서 넣으려고
        return -1;
    return 0;
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case1 : 내 앞의 푸터가 할당, 내 뒤의 헤더가 할당
    if(prev_alloc && !next_alloc){
        return bp; //병합불가로 바로 반환
    } 
    // case2 : 내 앞의 푸터가 할당, 내 뒤의 헤더가 미할당
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0)); // 헤더의 사이즈, 할당상태 변경
        PUT(FTRP(bp), PACK(size,0)); // 푸터의 사이즈, 할당상태 변경
    }
    // case3 : 내 앞의 푸터가 미할당, 내 뒤의 헤더가 할당
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); // 내 앞의 블럭의 헤더의 크기 +
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp); // bp를 이전블럭의 페이로드위치로 갱신(앞이 늘어났으니)
    }
    // case4 : 내 앞의 푸터가 미할당, 내뒤의 헤더가 미할당
    else{
        size+= GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp); //bp를 이전블럭의 페이로드위치로 갱신(앞이 늘어났으니)
    return bp;
}
}

// find_fit함수, frist-fit으로 구현
static void *find_fit(size_t asize){
    void *bp; //블록위치확인용 포인터

    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){ //힙의 유효한 첫번쨰 블록의 시작주소부터, 에필로그블록을 만날떄까지, 다음블럭으로 이동
        if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){ //헤당 블럭이 할당되지 않았고, asize보다 크면 해당블럭의 주소 반환
            return bp;
        }
    }
    return NULL; // No fit
}


//place 함수
static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize,1));//현재 크기를 헤더에 집어넣고
        PUT(FTRP(bp), PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize,0)); //현재크기를 제외한 사이즈를 미할당으로 갱신
        PUT(FTRP(bp), PACK(csize-asize,0));
    }
    else{ // 분할불가일경우
        PUT(HDRP(bp), PACK(csize,1)); //해당블럭에 사이즈를 갱신
        PUT(FTRP(bp), PACK(csize,1));
    }
}



static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));

    //힙 확장후에는 병합
    return coalesce(bp);
}



/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;       //할당할 블록 사이즈
    size_t extendsize;  
    char *bp;

    // 사이즈가 0이면 할당x
    if(size == 0)
        return NULL;
    
    // 패딩
    if(size <= DSIZE) // 사이즈가 8byte보다 작다면,
        asize = 2*DSIZE; // 최소블록조건인 16byte로 맞춤
    else // 사이즈가 8byte보다 크다면
        asize = DSIZE * ((size+(DSIZE)+(DSIZE-1))/DSIZE);
    
    // 가용리스트에서 검색 - find_fit 호출
    if((bp = find_fit(asize)) != NULL){
        place(bp, asize); // 호출가능하면 초과부분 분할
        return bp;
    }

    // 만약 맞는 메모리블럭을 못찾으면 더 많은 메모리블록을 배치함
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp,asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size,0));
    PUT(FTRP(ptr), PACK(size,0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














