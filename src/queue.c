#include "queue.h"
#include <stdlib.h>
#include <stdio.h>


Queue* new_queue()
{
	Queue* ret = calloc(sizeof(Queue),1);
	return ret;
}

void q_add(Queue* q, void* data)
{
	//create new node
	Node* node = malloc(sizeof(Node));
	node->data = data;
	node->next = NULL;
	node->prev = NULL;

	if (!q->front)
	{
		q->front = node;
		q->back = q->front;
		q->size++;
		q_print(q);
		return;
	}
	node->prev = q->back;
	q->back->next = node;
	q->back = node;
	q->size++;
	q_print(q);
}

void q_remove(Queue* q, void* data)
{
	if (!q->front)
		return;
	//edge case removing the front node
	if (q->front->data == data)
	{
		q->front = q->front->next;
		q->size--;
		return;
	}
	//everything else
	Node* curr = q->front;
	while (curr)
	{
		if (curr->data == data)
		{
			curr->prev->next = curr->next;
			curr->next->prev = curr->prev;
			free(curr);
			q->size--;
			return;
		}
		curr = curr->next;
	}
}

void q_print(Queue *q)
{
	printf("*** Queue (%d) ***\n", q->size);
		Node* curr = q->front;
	while (curr)
	{
		printf("<---prev:%p---| %p |---next:%p--->",
					curr->prev, curr->data, curr->next);
		curr = curr->next;
	}
	printf("**************\n\n");

}


