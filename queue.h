#ifndef MY_QUEUE_H
#define MY_QUEUE_H


struct QNode 
{ 
	int index;
	struct QNode *next;
}; 

struct LNode{
	unsigned int pid;
	unsigned int actual_pid;
	unsigned int frame;
	unsigned int page_numb;
	struct LNode* next;
};

struct Queue 
{ 
	struct QNode *front;
	struct QNode *rear;
	int count;
}; 


struct Queue *createQueue();
struct QNode *newNode(int index);
void enQueue(struct Queue* q, int index);
struct QNode *deQueue(struct Queue *q);
bool isQueueEmpty(struct Queue *q);
int getQueueCount(struct Queue *q);
void fifo_push(struct LNode**, int, int, int, int);
void print_list(struct LNode*);
struct LNode* fifo_pop(struct LNode**);


#endif
