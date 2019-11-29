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
static metadata_t* find_right(metadata_t*);
static metadata_t* find_left(metadata_t*);
static void merge(metadata_t* left, metadata_t* right);
static void double_merge(metadata_t* left, metadata_t* middle, metadata_t* right);
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
    my_malloc_errno = NO_ERROR;

    if (size > SBRK_SIZE - TOTAL_METADATA_SIZE) {
        my_malloc_errno = SINGLE_REQUEST_TOO_LARGE;
        return NULL;
    }

    if (size <= 0) {
        my_malloc_errno = NO_ERROR;
        return NULL;
    }

    metadata_t *curr = size_list;
    while (curr != NULL) {
        if (curr->size == size) {
            remove_from_size_list(curr);
            set_canary(curr);
            curr->next = NULL;
            return curr + 1;
        }

        curr = curr->next;
    }

    curr = size_list;
    while (curr != NULL) {
        if (curr->size >= MIN_BLOCK_SIZE + size) {
            return split_block(curr, size);
        }

        curr = curr->next;
    }

    metadata_t *metadata = my_sbrk(SBRK_SIZE);
    if (metadata == NULL) {
        my_malloc_errno = OUT_OF_MEMORY;
        return NULL;
    }

    metadata->next = NULL;

    metadata->size = SBRK_SIZE - TOTAL_METADATA_SIZE;
    if (find_left(metadata) != NULL && find_right(metadata) != NULL) {
        double_merge(find_left(metadata), metadata, find_right(metadata));
    } else if (find_right(metadata) != NULL) {
        merge(metadata, find_right(metadata));
    } else if (find_left(metadata) != NULL) {
        merge(find_left(metadata), metadata);
    } else {
        add_to_size_list(metadata);
    }

    return my_malloc(size);

    return NULL;
}

/* REALLOC
 * See PDF for documentation
 */
void *my_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return my_malloc(size);
    }

    if (ptr != NULL) {
        my_free(ptr);
        return NULL;
    }

    metadata_t *metadata = (metadata_t *) ((uint8_t *) ptr - sizeof(metadata_t));
    unsigned long calc = ((uintptr_t)metadata ^ CANARY_MAGIC_NUMBER) + 1890;
    if (metadata->canary != calc) {
        my_malloc_errno = CANARY_CORRUPTED;
        return NULL;
    }

    if (*((unsigned long *) ((uint8_t *) metadata + metadata->size + sizeof(metadata_t))) != calc) {
        my_malloc_errno = CANARY_CORRUPTED;
        return NULL;
    }

    void *newBlock = my_malloc(size);
    memcpy(newBlock, metadata + 1, size);
    return newBlock;
}

/* CALLOC
 * See PDF for documentation
 */
void *my_calloc(size_t nmemb, size_t size) {
    if (size == 0) {
        return NULL;
    }

    return my_malloc(nmemb * size);

    return NULL;
}

/* FREE
 * See PDF for documentation
 */
void my_free(void *ptr) {
    my_malloc_errno = NO_ERROR;
    if (ptr == NULL) {
        return;
    }

    metadata_t *metadata = (metadata_t *) ((uint8_t *) ptr - sizeof(metadata_t));

    unsigned long calc = ((uintptr_t)metadata ^ CANARY_MAGIC_NUMBER) + 1890;

    if (metadata->canary != calc) {
        my_malloc_errno = CANARY_CORRUPTED;
        return;
    }

    if (*((unsigned long *) ((uint8_t *) metadata + metadata->size + sizeof(metadata_t))) != calc) {
        my_malloc_errno = CANARY_CORRUPTED;
        return;
    }

    if (find_left(metadata) != NULL && find_right(metadata) != NULL) {
        double_merge(find_left(metadata), metadata, find_right(metadata));
    } else if (find_right(metadata) != NULL) {
        merge(metadata, find_right(metadata));
    } else if (find_left(metadata) != NULL) {
        merge(find_left(metadata), metadata);
    } else {
        add_to_size_list(metadata);
    }
}

static metadata_t* find_right(metadata_t *metadata) {
    metadata_t *curr = size_list;
    while (curr != NULL) {
        if ((uintptr_t) ((uint8_t *) metadata + metadata->size + TOTAL_METADATA_SIZE) == ((uintptr_t) curr)) {
            return curr;
        }

        curr = curr->next;
    }

    return NULL;
}

static metadata_t* find_left(metadata_t *metadata) {
    metadata_t *curr = size_list;
    while (curr != NULL) {
        if ((uintptr_t) ((uint8_t *) curr + curr->size + TOTAL_METADATA_SIZE) == ((uintptr_t) metadata)) {
            return curr;
        }

        curr = curr->next;
    }

    return NULL;
}

static void merge(metadata_t* left, metadata_t* right) {
    remove_from_size_list(left);
    remove_from_size_list(right);

    left->size += right->size + TOTAL_METADATA_SIZE;
    add_to_size_list(left);
}

static void double_merge(metadata_t* left, metadata_t* middle, metadata_t* right) {
    remove_from_size_list(left);
    remove_from_size_list(middle);
    remove_from_size_list(right);

    left->size += right->size + middle->size + 2 * TOTAL_METADATA_SIZE;
    add_to_size_list(left);
}

static metadata_t* split_block(metadata_t* block, size_t size) {
    metadata_t *pBlock = block;
    remove_from_size_list(block);
    block->size = block->size - size - TOTAL_METADATA_SIZE;
    set_canary(block);
    add_to_size_list(block);

    pBlock = ((metadata_t*) ((uint8_t*) block + TOTAL_METADATA_SIZE + block->size));
    pBlock->size = size;
    pBlock->next = NULL;
    set_canary(pBlock);
    return pBlock + 1;
}

static void add_to_size_list(metadata_t* add_block) {
    add_block->next = NULL;

    if (size_list == NULL) {
        size_list = add_block;
        return;
    }

    if (add_block->size < size_list->size) {
        add_block->next = size_list;
        add_block = size_list;
    }

    metadata_t *curr = size_list;
    while (curr->next != NULL && curr->next->size < add_block->size) {
        curr = curr->next;
    }

    add_block->next = curr->next;
    curr->next = add_block;
    set_canary(add_block);
}

static void remove_from_size_list(metadata_t* remove_block) {
    metadata_t *curr = size_list;

    if (curr == remove_block) {
        size_list = size_list->next;
    }

    while (curr != NULL) {
        if (curr->next == remove_block) {
            curr->next = remove_block->next;
        }

        curr = curr->next;
    }
}

static void set_canary(metadata_t* block) {
    block->canary = ((uintptr_t)block ^ CANARY_MAGIC_NUMBER) + 1890;
    *((unsigned long *) ((uint8_t *) block + TOTAL_METADATA_SIZE + block->size) - 1) = ((uintptr_t)block ^ CANARY_MAGIC_NUMBER) + 1890;
}