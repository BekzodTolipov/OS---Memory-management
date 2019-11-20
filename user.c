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
	srand(time(NULL) ^ getpid());
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
    //struct Clock time_to_request_release  = get_time_to_request_release_rsc(*sysclock);

	struct Clock start;
	struct Clock end;
	bool min_run_time = false;
	sem_lock(0);
	start.sec = end.sec = system_clock->sec;
	start.ns = end.ns = system_clock->ns;
	sem_release(0);
	int total_mem_ref = 0;
	bool allowed = true;
	while(1){
		//Waiting for master signal to get resources
	//	fprintf(stderr, "USER: Waiting for master (%d)\n", getpid());
		msgrcv(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), getpid(), 0);

		if(!min_run_time){
			sem_lock(0);
			end.sec = system_clock->sec;
			end.ns = system_clock->ns;
			sem_release(0);
			if((end.sec - start.sec) >= 1){
				min_run_time = true;
				//fprintf(stderr, "--USER: I (%d) ran for 1 second\n", pid);
			}
		}
		
		struct page pn;
		pn.page_numb = rand() % 32768 + 1;
		total_mem_ref++;

		if(pcb[pid].pg_tbl[(pn.page_numb>>10)].valid == 0){
			//fprintf(stderr, "USER REQUEST: Address is empty need to request at page(%u), 10 shift right(%d)\n", pn.page_numb, (pn.page_numb>>10));
			//Request
			msg.mtype = 1;
			msg.flag = 1;
			msg.is_request = 1;
			msg.page_number = pn.page_numb;	
			msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
			
			//Wait for grant
			msgrcv(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), getpid(), 0);
			fprintf(stderr, "USER GRANTED: Received message letting me know that its granted and changed addreess to (%d)\n", pcb[pid].pg_tbl[(pn.page_numb>>10)].address);
			msg.mtype = 1;
			msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
			allowed = false;
		//	if(msg.granted){
		//		continue;
		//	}
		}
//		else if(total_mem_ref > 1000){
//			fprintf(stderr, "USER FINISHED: Letting know that I am done, adress has (%u)\n\n", pcb[pid].pg_tbl[(pn.page_numb>>10)].address);
		//	sleep(2);
//			msg.mtype = 1;
//			msg.flag = 0;
//			msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
//			break;
//		}
		else{
			if(total_mem_ref < 1000){
			//	fprintf(stderr, "\nUSER MODIFIED: Letting know that I am done, total reference (%d)\n", total_mem_ref);
				msg.mtype = 1;
				msg.read_or_write = 1;	//Process is done
				msg.is_request = 0;
				msg.flag = 1;
				msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
			}//break;
		}

//		if(total_mem_ref > 1000){
//			msg.mtype = 1;
//			msg.flag = 0;
//			msgs
//		else{
//			//fprintf(stderr, "USER FINISHED: Letting know that I am done, adress has (%u)\n\n", pcb[pid].pg_tbl[(pn.page_numb>>10)].address);
//			msg.mtype = 1;
//			msg.read_or_write = 1;	//Process is done
//			msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
//			//break;
//		}
		if(allowed){
			//fprintf(stderr, "Checking total ref(%d)\n", total_mem_ref);
			if(total_mem_ref >= 1000){
			//	fprintf(stderr, "------------USER FINISHED: Sending message to OSS that I am done \n\n");
				msg.mtype = 1;
				msg.flag = 0;
				msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
			
				msgrcv(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), getpid(), 0);
				fprintf(stderr, "USER FINISHED: OSS let me to turn off \n\n");
				break;
			}
		//	else{
		//		fprintf(stderr, "USER FAIL\n\n");
		//	}
		}

		allowed = true;

//		fprintf(stderr, "USER FINISHED: Letting know that I am done\n\n");
		//Send a message to master that I got the signal and master should invoke an action base on my "choice"
//		msg.mtype = 1;
//		msg.flag = 0;	//Process is done

//		msgsnd(msg_q_id, &msg, (sizeof(struct Message) - sizeof(long)), 0);
//		break;
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
	if(sigaction(SIGUSR1, &sa1, NULL) == -1)
	{
		perror("ERROR");
	}

	struct sigaction sa2;
	sigemptyset(&sa2.sa_mask);
	sa2.sa_handler = &processHandler;
	sa2.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa2, NULL) == -1)
	{
		perror("ERROR");
	}
}
void processHandler(int signum)
{
	printf("%d: Terminated!\n", getpid());
	detach_from_shared_memory(pcb);
	detach_from_shared_memory(system_clock);
	exit(2);
}

/* ====================================================================================================
* Function    :  semaLock()
* Definition  :  Invoke semaphore lock of the given semaphore and index.
* Parameter   :  Semaphore Index.
* Return      :  None.
==================================================================================================== */
void sem_lock(int sem_index)
{
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = -1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}


/* ====================================================================================================
* Function    :  semaRelease()
* Definition  :  Release semaphore lock of the given semaphore and index.
* Parameter   :  Semaphore Index.
* Return      :  None.
==================================================================================================== */
void sem_release(int sem_index)
{	
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = 1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}
