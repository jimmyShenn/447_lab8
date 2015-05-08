/*
 * MIPS pipeline timing simulator
 *
 * ECE 18-447, Spring 2012 -- <cfallin>
 */

#ifndef _BP_H_
#define _BP_H_

#include <stdint.h>

typedef struct
{
    /* gshare */
    int ghr_bits;
    uint32_t ghr;
    uint8_t *pht; /* size 2^ghr_bits */

    /* BTB */
    int btb_size;
    int btb_bits;
    uint32_t *btb_tag;
    uint32_t *btb_dest;
    uint8_t *btb_valid;
    uint8_t *btb_cond;
} bp_t;

bp_t *bp_new(int ghr_bits, int btb_size);
void bp_destroy(bp_t *b);

void bp_predict(bp_t *b, uint32_t pc, uint8_t *branch, uint8_t *cond, uint8_t *taken, uint32_t *dest);
void bp_update(bp_t *b, uint32_t pc, uint8_t branch, uint8_t cond, uint8_t taken, uint32_t dest);

#endif
