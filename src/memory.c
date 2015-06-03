
/**
 *  microcsp   
 *  Copyright (c) 2015 Michael E. Goldsby
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "memory.h"
#include "hardware.h"
#include <stdio.h>
#include <stdlib.h>

// maximum number of memory allocation sizes
#define NALLOC  16

//blocks that have been allocated and released
static ChainedBlock_p procmemlist[NALLOC];  

// length of block for each index, in bytes
static uint16_t procmemlen[NALLOC] = 
    { 8, 12, 16, 24, 32, 40, 56, 58, 
     64, 70, 80, 96, 112, 128, 160, 192 };

// what's left, in one big block
static uint32_t taillen;    // in bytes
static char *tail;                 

/**
 * Find index of smallest allocation >= given size (bytes)
 */
unsigned int find_mem_index(unsigned int size)
{
    int8_t i;
    for (i = 0; procmemlen[i]; i++) {
        if (procmemlen[i] >= size)
            break;
    }
    if (procmemlen[i])
        return i;
    else
        error("No memory block large enough");
}

/** 
 * Allocate block of length implied by index
 * input:   index    1..NALLOC-1
 * output:  array of words of size corresponding to index
 */
char *allocate_mem(unsigned int index)
{
    DISABLE;
    ChainedBlock_p block = procmemlist[index];
    if (block != NULL)
    {
        //previously allocated block available, remove it from list
        procmemlist[index] = block->next;
    }
    else 
    {
        uint16_t len = procmemlen[index];
        if (taillen >= len)
        {
            // tail is big enough, take block from tail
            block = (ChainedBlock_p)tail;
            tail += len;
            taillen -= len;
            //Printf("%d left\n", taillen);
        }
        else
        {
            // out of memory
            block = NULL;
            error("Out of memory");
        }
    }
    
    ENABLE;
    return (char *)block;
}

/**
 * Release allocated block.
 * input:   index     1..NALLOC-1
 *          addr      ptr to allocated block
 */
void release_mem(unsigned int index, char *addr)
{
    // put released block at head of list for its size
    DISABLE;
    ChainedBlock_p block = (ChainedBlock_p)addr;
    block->next = procmemlist[index];
    procmemlist[index] = block;
    ENABLE;
}

/**
 * Initializes the memory system.
 * input:    memlen   size of total dynamic memory allocation, in bytes
 */
void memory_init(uint32_t memlen)
{
    // set allocatable lengths, none allocated yet
    int i;
    for (i = 0; i < NALLOC; i++)
    {
        procmemlist[i] = NULL;
    }
    
    tail = (char *)Malloc(memlen);
    if (tail == NULL) error("memory_init Malloc");
    taillen = memlen;
}


