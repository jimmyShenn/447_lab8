/*
 * MIPS pipeline timing simulator
 *
 * ECE 18-447, Spring 2012 -- <cfallin>
 */

#include "bp.h"
#include <stdlib.h>
#include <stdio.h>

#define log2(i) (__builtin_ffs( (i) ) - 1)

bp_t *bp_new(int ghr_bits, int btb_size)
{
    bp_t *b = calloc(sizeof(bp_t), 1);
    b->ghr_bits = ghr_bits;
    b->btb_size = btb_size;
    b->btb_bits = log2(btb_size);

    b->ghr = 0;
    b->pht = calloc(1 << ghr_bits, sizeof(uint8_t));
    b->btb_tag = calloc(btb_size, sizeof(uint32_t));
    b->btb_dest = calloc(btb_size, sizeof(uint32_t));
    b->btb_valid = calloc(btb_size, sizeof(uint8_t));
    b->btb_cond = calloc(btb_size, sizeof(uint8_t));

    return b;
}

void bp_destroy(bp_t *b)
{
    if (b) {
        free(b->pht);
        free(b->btb_tag);
        free(b->btb_dest);
        free(b->btb_valid);
        free(b->btb_cond);
        free(b);
    }
}

void bp_predict(bp_t *b, uint32_t pc, uint8_t *branch, uint8_t *cond, uint8_t *taken, uint32_t *dest)
{
    *branch = 0;
    *cond = 0;
    *taken = 0;
    *dest = 0;

    /* Look up in BTB */
    int btb_idx = (pc >> 2) & ((1 << b->btb_bits) - 1);

    if (b->btb_valid[btb_idx] && (b->btb_tag[btb_idx] == pc)) {
        *branch = 1;
        *cond = b->btb_cond[btb_idx];
        *dest = b->btb_dest[btb_idx];
    }

    if (*branch && *cond) {
        /* Look up in directional predictor */
        int mask = (1 << b->ghr_bits) - 1;
        int gshare_idx = ((pc >> 2) & mask) ^ (b->ghr & mask);

        *taken = (b->pht[gshare_idx] >= 2);
    }
    else if (*branch)
        *taken = 1;
}

void bp_update(bp_t *b, uint32_t pc, uint8_t branch, uint8_t cond, uint8_t taken, uint32_t dest)
{
    /* Update BTB */
    int btb_idx = (pc >> 2) & ((1 << b->btb_bits) - 1);
    if (!branch) {
        /* we were incorrectly predicted as a branch. Clear BTB entry. */
        b->btb_valid[btb_idx] = 0;
    }
    else {
        /* we are a branch. Make sure BTB is up-to-date. */
        b->btb_valid[btb_idx] = 1;
        b->btb_cond[btb_idx] = cond;
        b->btb_dest[btb_idx] = dest;
        b->btb_tag[btb_idx] = pc;
    }

    /* Update gshare directional predictor */
    int mask = (1 << b->ghr_bits) - 1;
    int gshare_idx = ((pc >> 2) & mask) ^ (b->ghr & mask);

    if (branch && cond) {
        if (taken) {
            if (b->pht[gshare_idx] < 3) b->pht[gshare_idx]++;
        } else {
            if (b->pht[gshare_idx] > 0) b->pht[gshare_idx]--;
        }
    }

    /* update global history register */
    if (branch && cond)
        b->ghr = ((b->ghr << 1) | (taken ? 1 : 0)) & mask;
}
