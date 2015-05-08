#include "cache.h"
#include "shell.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define MEM_STAGE       0
#define FETCH_STAGE     1

#define CLOSED_ROW  0
#define ROW_HIT     1
#define ROW_MISS    2

//#define DEBUG
//#define TEST

typedef struct
{
    uint32_t cycle; //which cycle it was issued
    uint8_t stage;  // which stage in the cycle it was issued
    uint8_t bank;   //which bank its going to
    uint32_t row;   //which row its going to
} Request;

typedef struct
{
    uint8_t valid;
    uint8_t done;
    uint32_t address;
    Request *request;
} MSHR;

typedef struct
{
    int32_t busy;
    int32_t active_row;
} DRAM_bank;

typedef struct
{
    uint32_t* words;
    uint32_t size;
} DRAM_row;

typedef struct
{
    DRAM_bank* banks;
    Queue request_queue;
} Memory_Controller;


/* general access function */
void mem_hierarchy_init(int sets, int ways, int block, int banks);
uint32_t mem_hierarchy_request(uint32_t addr, uint8_t stage);
void sched_mem_req(uint32_t *mem_stage, uint32_t *fetch_stage);
void L2_fill(uint32_t addr);
uint8_t schedulable(Request* req);
uint32_t schedule_request(Request* req);
uint8_t row_buff_hit(Request* req);
Request* DRAM_request(uint32_t addr, uint8_t stage);
void resolve_mem_reqs();
uint8_t row_buffer_status(Request *req);


/* L2 functions */

