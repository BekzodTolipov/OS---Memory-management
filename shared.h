fndef MY_SHARED_H
#define MY_SHARED_H
#include <stdbool.h>

typedef unsigned int uint;

struct SharedClock 
{
	uint sec;
	uint ns;
};


struct Message{
	long mtype;
	int pid;
	pid_t actual_pid;
	int flag;	//0 : isDone | 1 : isQueue
	bool is_request;
	bool is_release;
	//bool isSafe;
	char message[MAXCHAR];
};


typedef struct{
	uint address : 8;
	uint protn : 4;
	uint dirty : 1;
	uint ref : 1;
	uint valid : 1;
}pg_tbl_ent_t;


struct process_control_block{
	int pid;
	pid_t actual_pid;
	int request[MAX_RESOURCE];
	int release[MAX_RESOURCE];
	pg_tbl_ent_t pg_tbl[32];
};


#endif
