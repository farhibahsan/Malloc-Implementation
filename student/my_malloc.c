/*
 * CS 2110 Spring 2019
 * Author:
 */

/* we need this for uintptr_t */
#include <stdint.h>
/* we need this for memcpy/memset */
#include <string.h>
/* we need this to print out stuff*/
#include <stdio.h>
/* we need this for the metadata_t struct and my_malloc_err enum definitions */
#include "my_malloc.h"
/* include this for any boolean methods */
#include <stdbool.h>

/*Function Headers
 * Here is a place to put all of your function headers
 * Remember to declare them as static
 */
//static metadata_t* find_right(metadata_t*);
//static metadata_t* find_left(metadata_t*);
//static void merge(metadata_t* left, metadata_t* right);
//static void double_merge(metadata_t* left, metadata_t* middle, metadata_t* right);
static metadata_t* split_block(metadata_t* block, size_t size);
static void add_to_size_list(metadata_t* add_block);
static void remove_from_size_list(metadata_t* remove_block);
static void set_canary(metadata_t* block);

/* Our freelist structure - our freelist is represented as a singly linked list
 * the size_list orders the free blocks by size in ascending order
 */

metadata_t *size_list;

/* Set on every invocation of my_malloc()/my_free()/my_realloc()/
 * my_calloc() to indicate success or the type of failure. See
 * the definition of the my_malloc_err enum in my_malloc.h for details.
 * Similar to errno(3).
 */
enum my_malloc_err my_malloc_errno;

/* MALLOC
 * See PDF for documentation
 */
void *my_malloc(size_t size) {
    if (size > SBRK_SIZE - TOTAL_METADATA_SIZE) {
        my_malloc_errno = SINGLE_REQUEST_TOO_LARGE;
        return NULL;
    }

    metadata_t *curr = size_list;
    while (curr != NULL) {
        if (curr->size == size) {
            remove_from_size_list(curr);
            set_canary(curr);
            return ((uint8_t *) (curr + 1));
        } else if (curr->next == NULL) {
            if (curr->next->size - (size + TOTAL_METADATA_SIZE) >= MIN_BLOCK_SIZE) {
                metadata_t *returner = split_block(curr->next, size);
                set_canary(returner);
                return (((uint8_t *) returner + 1));
            }
        } else {
            my_sbrk(SBRK_SIZE);
            my_malloc(size);
        }
    }

    return NULL;
}

/* REALLOC
 * See PDF for documentation
 */
void *my_realloc(void *ptr, size_t size) {
    UNUSED_PARAMETER(ptr);
    UNUSED_PARAMETER(size);
    return NULL;
}

/* CALLOC
 * See PDF for documentation
 */
void *my_calloc(size_t nmemb, size_t size) {
    UNUSED_PARAMETER(nmemb);
    UNUSED_PARAMETER(size);
    return NULL;
}

/* FREE
 * See PDF for documentation
 */
void my_free(void *ptr) {
    UNUSED_PARAMETER(ptr);
}
/*
static metadata_t* find_right(metadata_t *metadata) {
    metadata_t *curr = size_list;
    while (curr != NULL) {
        if (metadata->next == curr && ((uint8_t *) (metadata + 1)) == ((uint8_t *) curr)) {
            return curr;
        }

        curr = curr->next;
    }

    return NULL;
}

static metadata_t* find_left(metadata_t *metadata) {
    metadata_t *curr = size_list;
    while (curr != NULL) {
        if (curr->next == metadata && (((uint8_t *S) (curr + 1)) == ((uintptr *) metadata))) {
            return curr;
        }

        curr = curr->next;
    }

    return NULL;
}

static void merge(metadata_t* left, metadata_t* right) {
    metadata_t *pLeft = left;
    remove_from_size_list(left);
    metadata_t *pRight = right;
    remove_from_size_list(right);

    pLeft->size = pLeft->size + pRight->size + TOTAL_METADATA_SIZE;
    add_to_size_list(pLeft);
}

static void double_merge(metadata_t* left, metadata_t* middle, metadata_t* right) {
    metadata_t *pLeft = left;
    remove_from_size_list(left);
    metadata_t *pMiddle = middle;
    remove_from_size_list(middle);
    metadata_t *pRight = right;
    remove_from_size_list(right);

    pLeft->size = pLeft->size + pRight->size + pMiddle->size + 2 * TOTAL_METADATA_SIZE;
    add_to_size_list(pLeft);
}
*/
static metadata_t* split_block(metadata_t* block, size_t size) {
    metadata_t *pBlock = block;
    remove_from_size_list(block);
    pBlock->size = pBlock->size - size - TOTAL_METADATA_SIZE;
    add_to_size_list(pBlock);

    return ((metadata_t*) ((uint8_t*) block) + TOTAL_METADATA_SIZE + block->size - size);
}

static void add_to_size_list(metadata_t* add_block) {
    metadata_t *curr = size_list;
    while (curr != NULL) {
        if (curr->next->size > add_block->size) {
            add_block->next = curr->next;
            curr->next = add_block;
            curr = NULL;
        } else {
            curr = curr->next;
        }
    }
}

static void remove_from_size_list(metadata_t* remove_block) {
    metadata_t *curr = size_list;
    while (curr != NULL) {
        if (((uint8_t *) curr->next) == ((uint8_t *) remove_block)) {
            curr->next = remove_block->next;
        }

        curr = curr->next;
    }
}

static void set_canary(metadata_t* block) {
    block->canary = ((uintptr_t)block ^ CANARY_MAGIC_NUMBER) + 1890;
    *(((uint8_t *) block + 1) + block->size) = ((uintptr_t)block ^ CANARY_MAGIC_NUMBER) + 1890;
}