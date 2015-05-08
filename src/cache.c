/*
 * MIPS pipeline timing simulator
 *
 * ECE 18-447, Spring 2012 -- <cfallin>
 */

#include "cache.h"
#include "shell.h"
#include <stdlib.h>
#include <stdio.h>

#define DEBUG
//#define TEST

#define log2(i) (__builtin_ffs( (i) ) - 1)

cache_t *cache_new(int sets, int ways, int block)
{
    cache_t *c = calloc(sizeof(cache_t), 1);

    c->sets = sets;
    c->ways = ways;
    c->log2sets = log2(sets);
    c->log2block = log2(block);

    c->present = calloc(sizeof(uint8_t), sets * ways);
    c->tag = calloc(sizeof(uint32_t), sets * ways);

    return c;
}

void cache_destroy(cache_t *c)
{
    if (c) {
        free(c->present);
        free(c->tag);
        free(c);
    }
}

int cache_probe(cache_t *c, uint32_t addr)
{
  //printf("KC:Probe %x\n", addr);
    uint32_t block = (addr >> c->log2block);
    uint32_t set = block & ((1UL << c->log2sets) - 1);

    uint8_t *setpres = &c->present[c->ways * set];
    uint32_t *settag = &c->tag[c->ways * set];

    for (int i = 0; i < c->ways; i++) {
        if (setpres[i] && (settag[i] == block)) {
            int j;
            for (j = i; j > 0; j--) {
                setpres[j] = setpres[j-1];
                settag[j] = settag[j-1];
            }
            setpres[0] = 1;
            settag[0] = block;
            return 1;
        }
    }

    return 0;
}

int cache_update(cache_t *c, uint32_t addr)
{
  //printf("KC:Update %x\n", addr);
    uint32_t block = (addr >> c->log2block);
    uint32_t set = block & ((1UL << c->log2sets) - 1);

    uint8_t *setpres = &c->present[c->ways * set];
    uint32_t *settag = &c->tag[c->ways * set];

    int hit = 0;

    int i;
    int firstempty = -1;
    for (i = 0; i < c->ways; i++) {
        if (setpres[i] && (settag[i] == block)) {
            int j;
            for (j = i; j > 0; j--) {
                setpres[j] = setpres[j-1];
                settag[j] = settag[j-1];
            }
            setpres[0] = 1;
            settag[0] = block;
            hit = 1;
            break;
        }
        if (!setpres[i] && firstempty == -1)
            firstempty = i;
    }

    if (!hit) {
        if (firstempty != -1) {
            for (i = firstempty; i > 0; i--) {
                setpres[i] = setpres[i-1];
                settag[i] = settag[i-1];
            }
            setpres[0] = 1;
            settag[0] = block;
            hit = 0;
        }
        else {
            for (i = c->ways - 1; i > 0; i--) {
                setpres[i] = setpres[i - 1];
                settag[i] = settag[i - 1];
            }
            setpres[0] = 1;
            settag[0] = block;
            hit = 0;
        }
    }

#ifdef DEBUG
    printf("Req (set %d) block %d: hit %d\nTags:", set, block, hit);
    for (i = 0; i < c->ways; i++)
        printf(" %d (pres %d)", settag[i], setpres[i]);
    printf("\n");
#endif

    return hit;
}

#ifdef TEST

int main()
{
    cache_t *c = cache_new(1, 4, 64);

    int seq[] = { 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 2, 3, 4, 5 };

    int i;
    for (i = 0; i < sizeof(seq)/sizeof(int); i++) {
        cache_probe(c, 64 * seq[i]);
    }

    cache_destroy(c);
}

#endif
