#include "malloc.h"

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

typedef struct _Block {
    /*
     * Pointer to the header of the next free block.
     * Only valid if this block is also free.
     * This is null for the last Block of the free list.
     */
    struct _Block *next;

    /*
     * Our header should always have a size of 16 Bytes.
     * This is just for 32 bit systems.
     */
    uint8_t padding[8 - sizeof(void*)];

    /*
     * The size of this block, including the header
     * Always a multiple of 16 bytes.
     */
    uint64_t size;

    /*
     * The data area of this block.
     */
    uint8_t data[];
} Block;

#define HEADER_SIZE sizeof(Block)
#define INV_HEADER_SIZE_MASK ~((uint64_t)HEADER_SIZE - 1)
#define ALLOCATED_BLOCK_MAGIC (Block*)(0xbaadf00d)
/*
 * This is the heap you should use.
 * 16 MiB heap space per default. The heap does not grow.
 */
#define HEAP_SIZE (HEADER_SIZE * 1024 * 1024)
uint8_t __attribute__ ((aligned(HEADER_SIZE))) _heapData[HEAP_SIZE];

/*
 * This should point to the first free block in memory.
 */
Block *_firstFreeBlock;

/*
 * Initializes the memory block. You don't need to change this.
 */
void initAllocator()
{
    _firstFreeBlock = (Block*)&_heapData[0];
    _firstFreeBlock->next = NULL;
    _firstFreeBlock->size = HEAP_SIZE;
}

/*
 * Gets the next block that should start after the current one.
 */
static Block *_getNextBlockBySize(const Block *current)
{
    static const Block *end = (Block*)&_heapData[HEAP_SIZE];
    Block *next = (Block*)&current->data[current->size - HEADER_SIZE];

    assert(next <= end);
    return (next == end) ? NULL : next;
}

/*
 * Dumps the allocator. You should not need to modify this.
 */
void dumpAllocator()
{
    Block *current;

    printf("All blocks:\n");
    current = (Block*)&_heapData[0];
    while (current) {
        assert((current->size & INV_HEADER_SIZE_MASK) == current->size);
        assert(current->size > 0);

        printf("  Block starting at %" PRIuPTR ", size %" PRIu64 "\n",
            ((uintptr_t)(void*)current - (uintptr_t)(void*)&_heapData[0]),
            current->size);

        current = _getNextBlockBySize(current);
    }

    printf("Current free block list:\n");
    current = _firstFreeBlock;
    while (current) {
        printf("  Free block starting at %" PRIuPTR ", size %" PRIu64 "\n",
            ((uintptr_t)(void*)current - (uintptr_t)(void*)&_heapData[0]),
            current->size);

        current = current->next;
    }
}

/*
 * Round the integer up to the block header size (16 Bytes).
 */
uint64_t roundUp(uint64_t n)
{
    int i = 1;
    while((uint64_t)(16 * i) < n) i++;
    return 16 * i;
}

void *my_malloc(uint64_t size)
{
    uint64_t requestedSize = roundUp(size) + HEADER_SIZE;
    Block *nextBlock = _firstFreeBlock;
    while(nextBlock != NULL) {
        // if entered make allocation
        if(nextBlock->size <= requestedSize) {
            // new block's size fits perfectly available block
            if(nextBlock->size == requestedSize) {
                // new free block
                _firstFreeBlock = nextBlock->next;
                // way to mark the next block as allocated
               _firstFreeBlock->next = ALLOCATED_BLOCK_MAGIC;
               return &nextBlock->data[0];
            }
            // free block is larger. split it into 2 blocks
            const uint64_t rest = nextBlock->size - requestedSize;
            nextBlock->size = size;
            // new first free block should be calculated
            _firstFreeBlock = _getNextBlockBySize(nextBlock);
            _firstFreeBlock->size = rest;
            _firstFreeBlock->next = nextBlock->next;
            return &nextBlock->data[0];
        }
        // no block large enough found. check next free block
        nextBlock = nextBlock->next;
    }
    return NULL;
}

void merge(Block* freeBlock) {
    if(freeBlock->next != NULL) {
            Block *nextBlock = _getNextBlockBySize(freeBlock);
            if(nextBlock == freeBlock->next) {
                freeBlock->size += nextBlock->size;
                freeBlock->next = nextBlock->next;
            }
        }
}

void my_free(void *address)
{
    if(address == NULL) {
        return;
    }

    // Address is pointing to data block. Remove one Header size;
    Block *block = (Block*)(address) -1;
    Block *freeBlock = _firstFreeBlock;
    if(freeBlock == NULL || freeBlock > block) {
        _firstFreeBlock = block;
        block->next = freeBlock;
    } else {
        while (freeBlock->next != NULL && freeBlock->next < block) {
            freeBlock = freeBlock->next;
        }
        block->next = freeBlock->next;
        freeBlock->next = block;
    }
    merge(block);
    merge(freeBlock);
}


