#include <stdlib.h>     //exit()
#include <stdio.h>      //printf()
#include <stdbool.h>    //bool variable
#include <stdint.h>     //for uint32_t
#include <string.h>     //str function
#include <unistd.h>     //standard symbolic constants and types
#include "queue.h"


struct Queue *createQueue()
{
	struct Queue *q = (struct Queue *)malloc(sizeof(struct Queue));
	q->front = NULL;
	q->rear = NULL;
	q->count = 0;
	return q;
}


struct QNode *newNode(int index)
{ 
    struct QNode *temp = (struct QNode *)malloc(sizeof(struct QNode));
    temp->index = index;
    temp->next = NULL;
    return temp;
} 


void enQueue(struct Queue *q, int index) 
{ 
	//Create a new LL node
	struct QNode *temp = newNode(index);

	//Increase queue count
	q->count = q->count + 1;

	//If queue is empty, then new node is front and rear both
	if(q->rear == NULL)
	{
		q->front = q->rear = temp;
		return;
	}

	//Add the new node at the end of queue and change rear 
	q->rear->next = temp;
	q->rear = temp;
}


struct QNode *deQueue(struct Queue *q) 
{
	//If queue is empty, return NULL
	if(q->front == NULL) 
	{
		return NULL;
	}

	//Store previous front and move front one node ahead
	struct QNode *temp = q->front;
	free(temp);
	q->front = q->front->next;

	//If front becomes NULL, then change rear also as NULL
	if(q->front == NULL)
	{
		q->rear = NULL;
	}

	//Decrease queue count
	q->count = q->count - 1;
	return temp;
} 


bool isQueueEmpty(struct Queue *q)
{
	if(q->rear == NULL)
	{
		return true;
	}
	else
	{
		return false;
	}
}


int getQueueCount(struct Queue *q)
{
	return (q->count);	
}

/*
	FIFO push
*/
void fifo_push(struct LNode** head_ref, int pid, int actual_pid,  int frame, int page_numb){
	/* allocate node */
    struct LNode* new_node =
             (struct LNode*) malloc(sizeof(struct LNode));

    /* put in the key  */
    new_node->pid  = pid;
	new_node->actual_pid = actual_pid;
	new_node->frame = frame;
	new_node->page_numb = page_numb;

    /* link the old list off the new node */
    new_node->next = (*head_ref);

    /* move the head to point to the new node */
    (*head_ref)    = new_node;
}
/*
	FIFO pop
*/
struct LNode* fifo_pop(struct LNode** head){
	struct LNode* temp = (*head);
	(*head) = temp->next;
	return temp;
}

/* Function to print nodes in a given linked list. fpitr is used
   to access the function to be used for printing current node data.
   Note that different data types need different specifier in printf() */
void print_list(struct LNode *node)
{
    while (node != NULL)
    {
        fprintf(stderr, ", %d", (node->frame));
        node = node->next;
    }
    fprintf(stderr, "\n\n");
}

