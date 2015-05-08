/*
 * MIPS pipeline timing simulator
 *
 * ECE 18-447, Spring 2012 -- <cfallin>
 */

#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdint.h>

typedef struct
{
    int sets, ways;
    int log2sets;
    int log2block;

    uint8_t *present;
    uint32_t *tag;
} cache_t;

cache_t *cache_new(int sets, int ways, int block);
void cache_destroy(cache_t *c);

int cache_probe(cache_t *c, uint32_t addr);
int cache_update(cache_t *c, uint32_t addr);

#endif
