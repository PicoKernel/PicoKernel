/* SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 PicoKernel Project
 */

/**
 * @file memory.c
 * @author datenbar
 * @date 30-05-2026
 * @brief Implementation of memory allocator.
 * @ingroup kernel
 * @version 0.1.0
 *
 * @details
 * Implements a simple free-list allocator which owns a chunk of memory and
 * hands it out as requested.
 *
 * Design notes:
 * - Uses singly linked list to traverse headers.
 * - Uses free-list memory allocation with attached headers to memory blocks.
 *
 * Known limitations:
 * - Does not check for double-free (yet).
 * - Does not zero out the memory (yet).
 * - Uses a canary and header so extra space is needed.
 * - Aligns the requested memory block so some bytes may go wasted.
 * - Doesn't coalesce backward(yet), may cause fragmentation.
 *
 * @todo Implement double-free detection.
 * @todo Implement calloc & realloc.
 * @todo Implement automatic HEAP_SIZE calibration.
 * @todo Remove is_free from header and use the lowest bit of size to save
 * memory.
 * @todo Implement backward coalescing in kfree().
 */

#include "memory.h"
#include "../panic/panic.h"
#include <stdint.h>
#include <stdio.h>

#define ALIGNMENT 8 /** Byte alignment boundary for all allocations. */
#define CANARY 0xDEADFACE /** Magic value written at the end of each allocated payload to detect heap overflow. */
#define HEAP_SIZE (300 * 1024) /** Total heap size in bytes. */

/**
 * @brief Heap block header.
 *
 * Stored immediately before each payload region.
 * Managed exclusively by the allocator.
 */
typedef struct block_header {
  size_t size;              /**< Payload size in bytes, excluding header and canary. */
  uint8_t is_free : 1;     /**< 1 if block is free, 0 if allocated. */
  struct block_header *next; /**< Pointer to the next block in the heap list. */
} block_header_t;

static block_header_t *heap_head = NULL;  /** Pointer to the first block in the heap. Anchor of the free list. */
static uint8_t heap[HEAP_SIZE]; /** Static heap buffer owned exclusively by the allocator. */

/**
 * @brief Aligns a size value to the nearest ALIGNMENT boundary.
 *
 * @param[in] size Raw size in bytes to align.
 *
 * @return Size rounded up to the nearest multiple of ALIGNMENT.
 */
static inline size_t align(size_t size) {
  return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

/**
 * @brief Initializes the memory allocator subsystem.
 *
 * Sets up the heap by placing a single free block spanning
 * the entire static heap buffer.
 *
 * @warning Must be called before any other allocator function.
 * @warning Calling this more than once will reset the heap and leak
 *          any previously allocated memory.
 *
 * @return Nothing.
 */
void allocator_init(void) {
  heap_head = (block_header_t *)heap;
  heap_head->size = HEAP_SIZE - sizeof(block_header_t);
  heap_head->is_free = 1;
  heap_head->next = NULL;
}

/**
 * @brief Allocates a block of memory from the heap.
 *
 * Searches the free list using a first-fit strategy. If a suitable
 * block is found, it is split if the remainder is large enough to
 * form a new free block. A canary value is written at the end of
 * the payload to detect heap overflows at free time.
 *
 * @param[in] size Number of bytes to allocate. Will be rounded up
 *                 to the nearest ALIGNMENT boundary.
 *
 * @return Pointer to the allocated payload on success.
 * @return NULL if size is 0, exceeds HEAP_SIZE, or no suitable
 *         block is available.
 */
void *kalloc(size_t size) {
  if (size == 0 || size > HEAP_SIZE) {
    return NULL;
  }
  size = align(size);
  block_header_t *current = heap_head;
  while (current != NULL) {
    if (current->is_free && current->size >= size) {
      if (current->size == size) {
        current->is_free = 0;
        uint32_t *canary = (uint32_t *)((uint8_t *)(current + 1) + size);
        *canary = CANARY;
        return (void *)(current + 1);
      }
      if (current->size > size + sizeof(block_header_t) + ALIGNMENT) {
        current->is_free = 0;
        uint32_t *canary = (uint32_t *)((uint8_t *)(current + 1) + size);
        *canary = CANARY;
        block_header_t *new_block = (block_header_t *)(canary + 1);
        new_block->is_free = 1;
        new_block->size = current->size - size - sizeof(block_header_t) - sizeof(uint32_t);
        new_block->next = current->next;
        current->next = new_block;
        current->size = size;
        return (void *)(current + 1);
      }
      uint32_t *canary = (uint32_t *)((uint8_t *)(current + 1) + size);
      *canary = CANARY;
      return (void *)(current + 1);
    }
    current = current->next;
  }
  return NULL;
}


/**
 * @brief Frees a previously allocated block of memory.
 *
 * Marks the block as free and attempts to coalesce it with the
 * immediately following block if it is also free. Validates the
 * canary value written at allocation time and triggers a kernel
 * panic if corruption is detected.
 *
 * @param[in] ptr Pointer to the payload of a block previously
 *                returned by kalloc(). Passing NULL is safe and
 *                has no effect.
 *
 * @warning Passing a pointer not returned by kalloc() is undefined
 *          behaviour.
 * @warning Double-free is not detected and will corrupt the heap.
 *
 * @return Nothing.
 */
void kfree(void *ptr) {
  if (ptr == NULL)
    return;
  block_header_t *header = (block_header_t *)ptr - 1;
  uint32_t *canary = (uint32_t *)((uint8_t *)ptr + header->size);
  if (*canary != CANARY) {
    kernel_panic("Allocator: Heap corruption detected!");
  }
  header->is_free = 1;
  if (header->next != NULL && header->next->is_free) {
    header->size = header->size + sizeof(uint32_t) + sizeof(block_header_t) + header->next->size;
    header->next = header->next->next;
  }
}
#ifdef KERNEL_DEBUG

/**
 * @brief Dumps the current state of the heap to stdout.
 *
 * Walks the entire block list and prints each block's address,
 * payload size, free status, and canary validity. Prints a summary
 * of total blocks, used bytes, and allocator overhead.
 *
 * @note Only available when KERNEL_DEBUG is defined.
 * @note Canary is reported as N/A for free blocks.
 *
 * @return Nothing.
 */
void kdump(void) {
  block_header_t *current = heap_head;
  int total_blocks = 0;
  int used_blocks = 0;
  int free_blocks = 0;
  size_t used_bytes = 0;
  printf("[KDUMP] heap @ %p | total: %zu bytes \n", current, (size_t)HEAP_SIZE);
  while (current != NULL) {
    uint32_t canary_val = current->is_free ? 0 : *(uint32_t *)((uint8_t *)(current + 1) + current->size);
    printf("[KDUMP] block %d | addr %p | size: %zu B | %s | canary: %s \n", total_blocks, current, current->size, current->is_free ? "free" : "used", current->is_free ? "N/A" : (canary_val == CANARY) ? "OK" : "CORRUPTED");
    total_blocks++;
    current->is_free ? free_blocks++ : used_blocks++;
    if (!current->is_free)
    used_bytes += current->size;
    current = current->next;
  }
  size_t overhead = (size_t)(total_blocks * (sizeof(block_header_t) + sizeof(uint32_t)));
  printf("[KDUMP] total_blocks: %d | used: %d | free: %d \n", total_blocks, used_blocks, free_blocks);
  printf("[KDUMP] payload used: %zu B | overhead: %zu B | total: %zu B | heap: %zu B \n", used_bytes, overhead, used_bytes + overhead, (size_t)HEAP_SIZE);
}
#endif
