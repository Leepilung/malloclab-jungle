
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
    "jungle",
    /* First member's full name */
    "sin",
    /* First member's email address */
    "sin@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

// 가용 리스트 조작을 위한 기본 상수 및 매크로 정의
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4 // word and header footer 사이즈를 byte로.
#define DSIZE 8 // double word size를 byte로
#define CHUNKSIZE (1<<8)   // EXTEND시킬 때 사이즈

#define MAX(x,y) ((x)>(y)? (x) : (y))

#define PACK(size,alloc) ((size)| (alloc))  // header, footer에 들어가는 값을 return 해준다.

// address p위치에 words를 read와 write를 진행함.
#define GET(p) (*(unsigned int*)(p))    // p값, (unsigned int *) 형변환으로 한칸당 단위
#define PUT(p,val) (*(unsigned int*)(p)=(val))  // 형변환된 값의 val을 p에 넣는다

#define GET_SIZE(p) (GET(p) & ~0x7) // ex 0x7 -> 000000111
#define GET_ALLOC(p) (GET(p) & 0x1) // 맨끝의 값(할당된 포인트만 가져온다)

#define HDRP(bp) ((char*)(bp) - WSIZE)  // 헤더위치 구하는 용도
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // footer 위치.

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp)-WSIZE))) // 다음 블록 위치 찾는것
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // 이전 블록 위치 찾는것

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size =  GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc){ 
        return bp;
    }
    else if (prev_alloc && !next_alloc){ 
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp),PACK(size,0)); 
        PUT(FTRP(bp), PACK(size,0)); 
    }
    else if(!prev_alloc && next_alloc){ 
        size+= GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0)); 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0)); 
        bp = PREV_BLKP(bp);
    }
    else { 
        size+= GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

// extend_heap으로 새 가용 블록으로 힙 확잦ㅇ하기

static void *extend_heap(size_t words){ 
    char *bp;
    size_t size;
    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    PUT(HDRP(bp), PACK(size,0)); 
    PUT(FTRP(bp),PACK(size,0)); 
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); 

    return coalesce(bp);
}


void mm_free(void *bp){ 
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp),PACK(size,0)); 
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}
// 최초 가용 블록으로 힙 생성

static char *heap_listp;

int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1){
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1));
    PUT(heap_listp + (3*WSIZE), PACK(0,1));
    heap_listp+= (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    return 0;
}

// first fit 검색을 수행
static void *find_fit(size_t asize){ 
    void *bp;
    for (bp= heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if (!GET_ALLOC(HDRP(bp)) && (asize<=GET_SIZE(HDRP(bp)))){
            return bp;
        }
    }
    return NULL; 
}




/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

static void place(void *bp, size_t asize){ // 요청한 블록을 가용 블록의 시작 부분에 배치, 나머지 부분의 크기가 최소 블록크기와 같거나 큰 경우에만 분할하는 함수.
    size_t csize = GET_SIZE(HDRP(bp));

    if ( (csize-asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize,1));
        PUT(FTRP(bp), PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize,0));
        PUT(FTRP(bp), PACK(csize-asize,0));
    }
    else{
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
    }
}

void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    size_t asize;
    size_t extendsize; 
    char *bp;

    if (size == 0) return NULL;

    if (size <= DSIZE){
        asize = 2*DSIZE;
    }
    else {
        asize = DSIZE* ( (size + (DSIZE)+ (DSIZE-1)) / DSIZE );
    }
    if ((bp = find_fit(asize)) != NULL){
        place(bp,asize);
        return bp;
    }
    extendsize = MAX(asize,CHUNKSIZE);
    if ( (bp=extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp,asize);
    return bp;
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
    copySize = *(size_t *)(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














