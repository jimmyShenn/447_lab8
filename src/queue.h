#include <stdint.h>

typedef struct node_t{
	void* data;
	struct node_t *next, *prev;
} Node;

typedef struct queue_t{
	uint32_t size;
	Node *front, *back;
} Queue;


Queue* new_queue();
void q_add(Queue* q, void* data);
void q_remove(Queue* q, void* data);
void q_print(Queue *q);


