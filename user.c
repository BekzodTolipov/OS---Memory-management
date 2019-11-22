/*********************************************************************************************
	Program Description: This program will be executed by child processes created in oss.
	It will start attaching to pcb and clock in shared memory.
	First process will check if page block is in main memory by checking its valid bit. If
	its invlaid than it will proceed to request oss to put it in main memory by incrementing
	page_fault.
	If its valid than process checks protection bit to see if it can access it to write into
	it. If process modified the frame, it will let oss know, so it can modify the dirty bit.
	After 1000+-100 references, process will determen if its going to terminate.

	Author: Bekzod Tolipov
	Date: 11/22/2019
*********************************************************************************************/

#include <stdlib.h>     //exit()
#include <stdio.h>      //printf()
#include <stdbool.h>    //bool variable
#include <stdint.h>     //for uint32_t
#include <string.h>     //str function
#include <unistd.h>     //standard symbolic constants and types

#include <stdarg.h>     //va macro
#include <errno.h>      //errno variable
#include <signal.h>     //signal handling
#include <sys/ipc.h>    //IPC flags
#include <sys/msg.h>    //message queue stuff
#include <sys/shm.h>    //shared memory stuff
#include <sys/sem.h>    //semaphore stuff, semget()
#include <sys/time.h>   //setitimer()
#include <sys/types.h>  //contains a number of basic derived types
#include <sys/wait.h>   //waitpid()
#include <time.h>       //time()

#include "global_constants.h"
#include "shared.h"
#include "message_queue.h"
#include "shared_memory.h"

// Global variables
static struct Message msg;
static int semid = -1;
static struct sembuf sema_operation;
static int exit_by_id;

// Shared memory
static int clock_shmid, pcb_shmid, msg_q_id = -1;
static struct Clock* system_clock;
static struct process_control_block* pcb;

// Prototypes
void sem_lock(int sem_index);
void sem_release(int sem_index);
void processInterrupt();
void processHandler(int signum);

struct page{
	unsigned int page_numb : 15;
};

int main(int argc, char *argv[]){
	processInterrupt();
	srand(time(NULL) ^ getpid());
	int max_ref = rand()%100 + 1000;
	/* =====Getting semaphore===== */
	key_t key = ftok("./oss.c", 21);
	semid = semget(key, 1, 0600);
	if(semid == -1)
	{
		fprintf(stderr, "USER ERROR: fail to attach a private semaphore! Exiting...\n");
		exit(1);
	}
    // Get shared memory IDs
    clock_shmid = atoi(argv[1]);
    pcb_shmid = atoi(argv[2]);
    msg_q_id = atoi(argv[3]);
    exit_by_id = atoi(argv[4]);
	int pid = exit_by_id;

	// Attach to shared memory
	system_clock = attach_shared_memory(clock_shmid, 1);
    pcb = attach_shared_memory(pcb_shmid, 0);

	sem_lock(0);
	sem_release(0);
	int total_mem_ref = 0;
	bool allowed = true;
	while(1){
		msgrcv(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), getpid(), 0);
		
		struct page pn;
		pn.page_numb = rand() % 32768 + 1;
		total_mem_ref++;

		if(pcb[pid].pg_tbl[(pn.page_numb>>10)].valid == 0){
			sprintf(msg.mtext, "USER REQUEST: PID(%d) is requesting page(%d) at %d.%d\n", pid, (pn.page_numb>>10), system_clock->sec, system_clock->ns);
			//Request
			msg.mtype = 1;
			msg.flag = 1;
			msg.is_request = 1;
			msg.page_number = pn.page_numb;	
			msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
			
			//Wait for grant
			msgrcv(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), getpid(), 0);
			msg.mtype = 1;
			msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
			allowed = false;
		}
		else{
			if(total_mem_ref < max_ref){
				if(pcb[pid].pg_tbl[(pn.page_numb>>10)].protn == 1){
					sprintf(msg.mtext, "\nUSER MODIFIED: PID(%d) Letting oss know that I modified block in memory at time %d.%d\n", pid, system_clock->sec, system_clock->ns);
					msg.read_or_write = 1;
				}
				else{	
					msg.read_or_write = 1;
				}
				msg.mtype = 1;
				msg.is_request = 0;
				msg.flag = 1;
				msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
			}
		}

		if(allowed){
			if(total_mem_ref >= max_ref){
				if(rand()%2){	// Randomly decide if finish or keep continue
					sprintf(msg.mtext, "------------USER FINISHED: PID(%d) Sending message to OSS that I finished my job at time %d.%d\n\n", pid, system_clock->sec, system_clock->ns);
					msg.mtype = 1;
					msg.flag = 0;
					msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
			
					msgrcv(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), getpid(), 0);
					break;
				}
				else{
					if(pcb[pid].pg_tbl[(pn.page_numb>>10)].protn == 1){
						sprintf(msg.mtext, "\nUSER MODIFIED: PID(%d) Letting oss know that I modified block in memory at time %d.%d\n", pid, system_clock->sec, system_clock->ns);
						msg.read_or_write = 1;
					}
					else{
						msg.read_or_write = 1;
					}
				
					sprintf(msg.mtext, "\nUSER MODIFIED: PID(%d) Letting oss know that I modified block in memory at time %d.%d\n", pid, system_clock->sec, system_clock->ns);
					msg.mtype = 1;
					msg.flag = 1;
					msg.is_request = 0;
					msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
				}
			}
		}

		allowed = true;
	}

	detach_from_shared_memory(pcb);
	detach_from_shared_memory(system_clock);
	return exit_by_id;
}

void processInterrupt()
{
	struct sigaction sa1;
	sigemptyset(&sa1.sa_mask);
	sa1.sa_handler = &processHandler;
	sa1.sa_flags = SA_RESTART;
	if(sigemptyset(&sa1.sa_mask) || sigaction(SIGTERM, &sa1, NULL) == -1)
	{
		fprintf(stderr, "ERROR: Failed to set up handler");
		perror("ERROR");
		exit(1);
	}
}

void processHandler(int signum)
{
	detach_from_shared_memory(pcb);
	detach_from_shared_memory(system_clock);
	exit(1);
}

void sem_lock(int sem_index)
{
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = -1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}

void sem_release(int sem_index)
{	
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = 1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}
