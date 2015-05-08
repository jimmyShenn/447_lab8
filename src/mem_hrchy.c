#include "shell.h"
#include "mem_hrchy.h"
#include <stdlib.h>
#include <stdio.h>

//#define DEBUG
//#define TEST

#define NUM_MSHR	16
#define log2(i) (__builtin_ffs( (i) ) - 1)


/* L2 cache */
cache_t L2_cache;
MSHR* L2_regs;
uint32_t num_free_regs;

/* DRAM */
uint8_t CA_bus;
uint32_t data_bus;

Memory_Controller MC;


/* mem_hierarchy_init - initialize the L2 cache, the registers and the memory
 *
 *		Parameters:
 *			- sets: number of sets of L2 cache
 *			- ways: associativity of L2 cache
 *			- block: block size of L2 cache
 *			- banks: number of banks in DRAM
 */
void mem_hierarchy_init(int sets, int ways, int block,
						int banks)
{
	//initialize busses free orginally
	CA_bus = 0;
	data_bus = 0;
	//initialize L2 cache
	num_free_regs = 0;
	L2_cache = *cache_new(sets, ways, block);
	L2_regs = calloc(sizeof(MSHR), NUM_MSHR);

	//initialize DRAM
	MC.banks = calloc(sizeof(DRAM_bank), banks);
	for (int i = 0; i < banks; i++)
	{
		MC.banks[i].active_row = -1;
		MC.banks[i].busy = -1;
	}
	MC.request_queue = *new_queue();
	
}



/* mem_hierarchy_request - the interface function called from the pipeline
 *							to handle L1 miss.
 *	Parameters:
 *		- addr: the address that caused the miss in the L1 cache
 *		- stage: whether the request came from data cache (mem stage) or
 *				icache (fetch stage)
 *	Returns: number of cycles that the pipeline needs to stall
 */
uint32_t mem_hierarchy_request(uint32_t addr, uint8_t stage)
{
	/*** handle L2 first ***/
	/* hit */ 
	if (cache_probe(&L2_cache, addr))
	{
#ifdef DEBUG
		printf("L2 hit\n");
#endif
		return 15;
	}
	/* miss */
	//check if the request is already being serviced, return how much stalling
	//time left
	for (uint32_t i = 0; i < num_free_regs; i++)
	{
		if (L2_regs[i].address == addr)
		{
#ifdef DEBUG
			printf("%x address already serviced\n", addr);
#endif
			Request* req = L2_regs[i].request;
			return MC.banks[req->bank].busy + 9;
		}
	}
	//allocate MSHR
	if (num_free_regs < 16)
	{
#ifdef DEBUG
		printf("L2 miss\n");
#endif
		L2_regs[num_free_regs].valid = 1;
		L2_regs[num_free_regs].address = addr;
		L2_regs[num_free_regs].request = DRAM_request(addr, stage);
		num_free_regs++;
		//5 + 5. the rest will be added by the MC at the end of the cycle
		return 10;
	}
	return 1;
}



/* DRAM_request - create a new request based on the type of access and adds it
 *					to the MC's request queue
 *
 *		Parameters:
 *			- addr: the miss address which gets parsed to the bank
 *			- stage: whether the request came from data cache (mem stage) or
 *					icache (fetch stage)
 */
Request* DRAM_request(uint32_t addr, uint8_t stage)
{
	Request* req = (Request*)malloc(sizeof(Request));
	req->bank = (addr >> 5) & 0x7;
	req->row = (addr >> 16) & 0xFFFF;
	req->stage = stage;
	q_add(&(MC.request_queue), req);

	printf("Arrival bank%d row%d\n", req->bank, req->row);
#ifdef DEBUG
        printf("create memory request: %x address\n", addr);
#endif
	return req;
}



/* mem_service_request - interface function called from the pipeline to handle
 *						scheduling a memory request every cycle
 *
 *		Parameters:
 *			- mem_stage: amount of cycles to stall a request from the data cache
 *			- fetch_stage: amount of cycles to stall a request from the icache
 */
void sched_mem_req(uint32_t *mem_stage, uint32_t *fetch_stage)
{
#ifdef DEBUG
	printf("DRAM banks stalling:\n");
	for (int i = 0; i < 8; i++)
		printf("bank_%d: %d  ", i, MC.banks[i].busy);
	printf("\n");

	printf("\nMSHR:%d\n", num_free_regs);
#endif

	//decrememnt cycle count for banks and buses 
	for (int i = 0; i < 8; i++)
	{
		if (MC.banks[i].busy > 0)
			MC.banks[i].busy--;
		if (CA_bus > 0)
			CA_bus--;
		if (data_bus > 0)
			data_bus--;
	}
	*fetch_stage = 0;
	*mem_stage = 0;
	Node* curr = MC.request_queue.front;
	if (!curr)
		return;
	Queue sched_q = *new_queue();
	//first check if schedulable
	while (curr)
	{
		if (schedulable((Request*)curr->data))
			q_add(&sched_q, curr->data);
		curr = curr->next;
	}

#ifdef DEBUG
        printf("schedule memory request: %d schedulable requests\n", sched_q.size);
#endif
	
	//if nothing is schedulable keep stalling
	if (sched_q.size == 0)
	{
		*fetch_stage = 1;
		*mem_stage = 1;
		return;
	}
	//if only one schedulable request
	if (sched_q.size == 1)
	{
		Request* req = (Request*)sched_q.front->data;
		uint32_t cycles = schedule_request(req);
		if (req->stage == MEM_STAGE)
		{
			*mem_stage = cycles;
		}
		else
		{
			*fetch_stage = cycles;
		}
		return;
	}

	//if row buffer hit
	curr = sched_q.front;
	while (curr) {
		if (!row_buff_hit((Request*)curr->data))
			q_remove(&sched_q, curr->data);
		curr = curr->next;
	}
	//schedule the earliest request
	Request* req = (Request*)sched_q.front->data;
	uint32_t cycles = schedule_request(req);
	if (req->stage == MEM_STAGE)
	{
		*mem_stage = cycles;
		*fetch_stage = 0;
	}
	else
	{
		*mem_stage = 0;
		*fetch_stage = cycles;
	}

}


/* schedule_request - takes in the request, analyses how many cycles it would
 *					stall for, and removes it from the queue. Changes bank state
 *
 *		Parameters:
 *			- req : the request to schedule
 *
 *		Returns: amount of cycles to stall the pipeline
 */
uint32_t schedule_request(Request* req)
{
	uint32_t ret = 0;
	uint8_t row_stat = row_buffer_status(req);
	switch(row_stat)
	{
		case CLOSED_ROW:
#ifdef DEBUG
			printf("sched req: closed row buffer\n");
#endif
			ret = 249;
			//set when buses will be used
			data_bus = 200;
			break;
		case ROW_HIT:
#ifdef DEBUG
			printf("sched req: row buffer hit\n");
#endif
			ret = 149;
			//set when the data bus will be used
			data_bus = 100;
			break;
		case ROW_MISS:
#ifdef DEBUG
			printf("sched req: row buffer miss\n");
#endif
			ret = 349;
			//set when the data bus will be used
			data_bus = 300;
			break;
	}
	
	//change bank to busy
	MC.banks[req->bank].busy = (int32_t)ret;
	MC.banks[req->bank].active_row = (int32_t)req->row;
	
	//change the command bus 
	CA_bus = 4;

	//remove the request from req queue
	q_remove(&MC.request_queue, req);

	printf("Scheduled bank%d row%d\n", req->bank, req->row);

	return ret;
}


/* row_buff_hit - checks whether the request will be a row buffer hit
 *
 *		Parameters:
 *			- req: the request
 *		Returns: 1 if row hit, 0 otherwise
 */
uint8_t row_buff_hit(Request* req)
{
	return (MC.banks[req->bank].active_row == (int32_t)req->row);
}

/* schedulable - checks whether the request can be scheduled by seeing if the
 *				busses and the bank it needs are available at the right cycles
 *
 *		Parameters:
 *			- req: the request
 *		Returns: 1 if schedulable, 0 otherwise
 */
uint8_t schedulable(Request* req)
{
	//is bank available
	if (MC.banks[req->bank].busy >= 0)
		return 0;

	//is command bus available
	if (CA_bus > 0)
		return 0;

	//is data bus available when it needs it
	uint32_t d_bus = 0;
	uint8_t row_stat = row_buffer_status(req);
	switch(row_stat)
	{
		case CLOSED_ROW:
			d_bus = 200;
			break;
		case ROW_HIT:
			d_bus = 100;
			break;
		case ROW_MISS:
			d_bus = 300;
			break;
	}
	//TODO make sure if its available the same cycle it brings back data
	if (d_bus <= data_bus)
		return 0;
	
	return 1;
}


/* L2_fill - updates the L2 cache at the appropriate time with the data
 *
 * Parameters:
 *		- addr: the address of the block that gets filled
 */
void L2_fill(uint32_t addr)
{
	//free MSHR register
	uint32_t num = num_free_regs;
	for (uint32_t i = 0; i < num; i++)
	{
		if (L2_regs[i].valid && L2_regs[i].address == addr)
		{
			L2_regs[i].valid = 0;
			L2_regs[i].address = 0;
		}
	}	

	num_free_regs--;
	//fill the cache
#ifdef DEBUG
	printf(" ** L2 fill ** \n");
#endif
	cache_update(&L2_cache, addr);
}

/* resolve_mem_reqs - gets called in the beginning of each cycle to check if
 *						there are any memory requests coming back from DRAM
 */
void resolve_mem_reqs()
{
	//loop through all the allocated mshrs, see if any requests' banks have 
	//finished servicing the request
	for (uint32_t i = 0; i < num_free_regs; i++)
	{
		Request req = *L2_regs[i].request;
		if (MC.banks[req.bank].busy == 0)
		{
			MC.banks[req.bank].busy--;
			
			printf("Resolved bank%d row%d\n", req.bank, req.row);
			L2_fill(L2_regs[i].address);
		}
	}
}

// row_buffer_status - analyses the request and checks what kind of row buffer
//						scenario it belongs to in its bank.
//
//		Parameters:
//			- req: the request to the memory bank
//		Returns:
//			- 0: if closed row buffer
//			- 1: if row buffer hit
//			- 2: if row buffer miss/conflict
uint8_t row_buffer_status(Request *req)
{
	//if closed row buffer
	if (MC.banks[req->bank].active_row < 0)
		return CLOSED_ROW;
	//if row buffer hit
	if (row_buff_hit(req))
		return ROW_HIT;
	//if row buffer miss
	return ROW_MISS;
}


