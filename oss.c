/************************************************************************************************
	Program Description: This program is designed to demonstrate how memory is managed in 
	this simulated operating system. The OSS will start with creating all necessary shared mem-
	mories and initialize. Default program will run FIFO page replacement algorithm when main
	memory is full and increment number of page_faults. Program has option to run with LRU page
	replacement algorithm by typing option -l at the run time.

	Author: Bekzod Tolipov
	Date: 11/22/2019
************************************************************************************************/

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
#include "queue.h"
#include "message_queue.h"
#include "shared_memory.h"

#define ONE_BILLION 1000000000

//static globals
static FILE *fptr = NULL;
static key_t key;
static struct Queue *queue;
static struct Clock fork_time;
unsigned int num_resources_granted = 0;
struct LNode* fifo_head = NULL;
struct LNode* lru_head = NULL;
unsigned int page_faults = 0;
unsigned int total_requests = 0;
unsigned int granted = 0;
unsigned int total_fifo = 0;
unsigned int total_lru = 0;
bool fifo_or_lru = true;
int child_pids[MAX_PROCESS];

//Shared memory
static int msg_q_id, semid, pcb_shmid, clock_shmid = -1;
static struct Message master_msg;
static struct Clock *system_clock = NULL;
static struct sembuf sema_operation;
static struct process_control_block *pcb = NULL;

//Fork
static int total_process = 0;
static pid_t child_pid = -1;
static unsigned char bit_map[3];
static unsigned char main_memory[MAIN_MEMORY_SIZE];

//Prototypes
static void setuptimer(int s);
static void setupinterrupt();
static void myhandler(int s);
unsigned int random_time_elapsed();
int get_free_frame();
int clear_and_pop();
void print_statistics();
void print_and_write(char* str);

void wait_for_all_children();
void cleanup_and_exit();
struct Clock get_fork_time_new_proc(struct Clock system_clock);
void __init_pcb_start(struct process_control_block *pcb);
void __init_pcb(struct process_control_block *pcb, int id, pid_t pid);
void sem_lock(int sem_index);
void sem_release(int sem_index);

//Clock prototypes
void incr_clock(struct Clock* Clock, int elapsed_sec);
struct Clock add_clocks(struct Clock c1, struct Clock c2);
int compare_clocks(struct Clock c1, struct Clock c2);
long double Clock_to_sec(struct Clock c);
struct Clock sec_to_Clock(long double sec);
struct Clock calculate_avg_time(struct Clock clk, int divisor);
struct Clock subtract_Clocks(struct Clock c1, struct Clock c2);
struct Clock ns_to_Clock(int ns);
struct Clock get_clock();
void set_clock(struct Clock* clk);
struct Clock get_fork_time_new_proc(struct Clock system_clock);

/*================================================
Main
================================================*/
int main(int argc, char *argv[]){
	char file_name[MAXCHAR] = "log.dat";
	int max_time = 5;
	int c;
	char dummy[MAXCHAR];
	srand(time(NULL));

	// Read the arguments given in terminal
	while ((c = getopt (argc, argv, "hlt:")) != -1){
		switch (c)
		{
			case 'h':
				printf("To run the program you have following options:\n\n[ -h for help]\n[ -l for LRU (default FIFO) ] [ -t change timer ]\nTo execute the file follow the code:\n./%s [ -h ] or any other options", argv[0]);
				return 0;
			case 'l':
				fifo_or_lru = false;
				break;
			case 't':
				strncpy(dummy, optarg, 255);
				max_time = atoi(dummy);
				break;
			default:
				fprintf(stderr, "ERROR: Wrong Input is Given!");
				abort();
		}
	}

	// Open log file for writing
    fptr = fopen(file_name, "w");
	// Validate if file opened correctly
	if(fptr == NULL){
		fprintf(stderr, "ERROR: Failed to open the file, terminating program\n");
		return 1;
	}
	setvbuf(fptr, NULL, _IONBF, 0);

	//Zero out all elements of bit map
	memset(bit_map, '\0', sizeof(bit_map));
	memset(main_memory, '\0', sizeof(main_memory));
	memset(child_pids, 0, sizeof(child_pids));

	setuptimer(max_time);
	// System Interrupt set up
    setupinterrupt();

	fork_time = get_clock();    // Holds time to schedule new process
    
    // Shared logical Clock
    // ==================================  Shared Logical Clock  ================================================== //
	key = ftok("./oss.c", 20);
    clock_shmid = get_shared_memory(key, sizeof(struct Clock));
    system_clock = (struct Clock*) attach_shared_memory(clock_shmid, 0);
    set_clock(system_clock);
	
    // ==================================  Shared Semaphore  ================================================== //
	key = ftok("./oss.c", 21);
	semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600);
	if(semid == -1)
	{
		fprintf(stderr, "MASTER ERROR: failed to create a new private semaphore! Exiting...\n");
		cleanup_and_exit();
		exit(EXIT_FAILURE);
	}
	
	// Initialize the semaphore(s) in our set to 1
	semctl(semid, 0, SETVAL, 1);	//Semaphore #0: for [shmclock] shared memory

    // =========================================  Shared pcb  ================================================== //
	//Allocate shared memory if doesn't exist, and check if can create one. Return ID for [pcbt] shared memory
	key = ftok("./oss.c", 22);
	size_t pcb_size = sizeof(struct process_control_block) * MAX_PROCESS;
	pcb_shmid = shmget(key, pcb_size, IPC_CREAT | 0600);
	if(pcb_shmid < 0)
	{
		fprintf(stderr, "MASTER ERROR: could not allocate [pcb] shared memory! Exiting...\n");
		cleanup_and_exit();
		exit(EXIT_FAILURE);
	}

	//Attaching shared memory and check if can attach it. If not, delete the [pcbt] shared memory
	pcb = shmat(pcb_shmid, NULL, 0);
	if(pcb == (void *)( -1 ))
	{
		fprintf(stderr, "MASTER ERROR: fail to attach [pcb] shared memory! Exiting...\n");
		cleanup_and_exit();
		exit(EXIT_FAILURE);	
	}

	//Init process control block table variable
	__init_pcb_start(pcb);

    // Shared resource message box for user processes to request/release resources 
	key = ftok("./oss.c", 22);
	msg_q_id = get_message_queue(key);

	queue = createQueue();

	// Get a time to fork first process at
    fork_time = get_fork_time_new_proc(*system_clock);
	*system_clock = fork_time;

	//===================================  Main Loop  ===========================================//
	unsigned int id = -1;
	bool is_bit_open = false;
	bool already_clean = false;
	char buffer[2000];
	while(1){
		if (compare_clocks(*system_clock, fork_time) >= 0) {	//Compare clock will return (a>b:1), (a==b:0), (a<b:-1)
			is_bit_open = false;
			int proc_count = 0;
			while(1){
				id = (id + 1) % MAX_PROCESS;
				uint32_t bit = bit_map[id / 8] & (1 << (id % 8));
				if(bit == 0){
					is_bit_open = true;
					break;
				}
				else{
					is_bit_open = false;
				}

				if(proc_count >= MAX_PROCESS - 1){
					break;
				}
				proc_count++;
			} //End of bit_map

			if(is_bit_open == true){
				if((child_pid = fork()) == 0){
					//Execute ./child
					char clock_id_in_char[10];
					char pcb_id[10];
					char msg_id[10];
					char p_id[5];
					
					sprintf(clock_id_in_char, "%d", clock_shmid);
					sprintf(pcb_id, "%d", pcb_shmid);
					sprintf(msg_id, "%d", msg_q_id);
					sprintf(p_id, "%d", id);

					char *exec_arr[6];
					exec_arr[0] = "./user";
					exec_arr[1] = clock_id_in_char;
					exec_arr[2] = pcb_id;
					exec_arr[3] = msg_id;
					exec_arr[4] = p_id;
					exec_arr[5] = NULL;
					
					execvp(exec_arr[0], exec_arr);
					perror("Child failed to execvp the command!");
					exit(1);
				}
				else if(child_pid < 0){
					fprintf(stderr, "i\n\nFork problem!!!\n\n");
					perror("Child failed to fork!\n");
					cleanup_and_exit();
					already_clean = true;
					break;
				}
				else{
					total_process++;
					bit_map[id / 8] |= (1 << (id % 8));
					child_pids[id] = child_pid;
					
					__init_pcb(&pcb[id], id, child_pid);
					//Add the process to highest queue
					enQueue(queue, id);

					//Display creation time
					sprintf(buffer, "\n\nMASTER: generating process with PID (%d) [%d] and putting it in queue at time %d.%d\n", pcb[id].pid, pcb[id].actual_pid, system_clock->sec, system_clock->ns);
					print_and_write(buffer);
					memset(buffer, 0, sizeof(buffer));
				//	fflush(fptr);
				}
			}
			fork_time = get_fork_time_new_proc(*system_clock);
		} //End of compare clock
		sem_lock(0);
		incr_clock(system_clock, random_time_elapsed());
		sem_release(0);

		// ===============================================   Traverse Queue   ==================================================== //
		struct QNode next;
		struct Queue *t_queue = createQueue();

		int current_iteration = 0;
		next.next = queue->front;
		while(next.next != NULL){
			sem_lock(0);
			incr_clock(system_clock, random_time_elapsed());
			sem_release(0);

			//Sending a message to a specific child to tell him it is his turn
			int q_id = next.next->index;
			master_msg.mtype = pcb[q_id].actual_pid;
			master_msg.pid = q_id;
			master_msg.actual_pid = pcb[q_id].actual_pid;
			msgsnd(msg_q_id, &master_msg, (sizeof(struct Message) - sizeof(long)), 0);

			//Waiting for the specific child to respond back
			msgrcv(msg_q_id, &master_msg, (sizeof(struct Message) - sizeof(long)), 1, 0);
			
			if(master_msg.flag == 0){	// Remove from queue process
				print_and_write(master_msg.mtext);
				sprintf(buffer, "MASTER: process with PID (%d) [%d] has finish running at my time %d.%d\n", master_msg.pid, master_msg.actual_pid, system_clock->sec, system_clock->ns);
				print_and_write(buffer);
					memset(buffer, 0, sizeof(buffer));
				master_msg.mtype = pcb[q_id].actual_pid;
				msgsnd(msg_q_id, &master_msg, (sizeof(struct Message) - sizeof(long)), 0);
				//Remove the process out of the queue
				struct QNode delete_node;
				delete_node.next = queue->front;
				while(delete_node.next != NULL)
				{
					if(delete_node.next->index != q_id)
					{
						enQueue(t_queue, delete_node.next->index);
					}

					//Point the pointer to the next queue element
					delete_node.next = (delete_node.next->next != NULL) ? delete_node.next->next : NULL;
				}

				//Reassigned the current queue
				while(!isQueueEmpty(queue))
				{
					deQueue(queue);
				}
				while(!isQueueEmpty(t_queue))
				{
					int i = t_queue->front->index;
					enQueue(queue, i);
					deQueue(t_queue);
				}

				//Point the pointer to the next queue element
				next.next = queue->front;
				int i;
				for(i = 0; i < current_iteration; i++)
				{
					next.next = (next.next->next != NULL) ? next.next->next : NULL;
				}
				continue;
			} //End of done flag
			sem_lock(0);
			incr_clock(system_clock, random_time_elapsed());
			sem_release(0);
			
			if(master_msg.read_or_write == true){
				sprintf(buffer, "\nUSER MODIFIED: PID(%d) User let me know that it modified block updating dirty bit at time %d.%d\n", q_id, system_clock->sec, system_clock->ns);
				print_and_write(buffer);
				pcb[q_id].pg_tbl[(master_msg.page_number>>10)].dirty = 1;
				if(!fifo_or_lru){
				//	total_lru++;
					struct LNode* move_down = fifo_pop(&lru_head);
					fifo_push(&lru_head, move_down->pid, move_down->actual_pid, move_down->frame, move_down->page_numb);
				}
			} //End of READ/WRITE

			sem_lock(0);
			incr_clock(system_clock, random_time_elapsed());
			sem_release(0);

			// Check if it is a request
			if(master_msg.is_request == true)
			{	
				print_and_write(master_msg.mtext);
				granted++;
				total_requests++;
				sprintf(buffer, "MASTER REQUEST: process with PID (%d) [%d] is REQUESTING frame from main memory. Granting request... at time %d.%d\n",
					master_msg.pid, master_msg.actual_pid, system_clock->sec, system_clock->ns);
				print_and_write(buffer);
				memset(buffer, 0, sizeof(buffer));
				page_faults++;
				pcb[q_id].pg_tbl[(master_msg.page_number>>10)].valid = 1;
				int frame = get_free_frame();
				if(frame != -1){
					if(fifo_or_lru){
						fifo_push(&fifo_head, q_id, master_msg.actual_pid, frame, (master_msg.page_number>>10));
					}
					else{
						fifo_push(&lru_head, q_id, master_msg.actual_pid, frame, (master_msg.page_number>>10));
					}
				}
				else{
					if(fifo_or_lru){
						//FIFO Stuff
						page_faults++;
						frame = clear_and_pop();
	memset(main_memory, '\0', sizeof(main_memory));
						total_fifo++;
						fifo_push(&fifo_head, q_id, master_msg.actual_pid, frame, (master_msg.page_number>>10));
					}
					else{
						//LRU Stuff
						page_faults++;
						frame = clear_and_pop();                    
						total_lru++;
						fifo_push(&lru_head, q_id, master_msg.actual_pid, frame, (master_msg.page_number>>10));
					}
					//Give the frame to new process
					pcb[q_id].pg_tbl[(master_msg.page_number>>10)].address = frame;
					sprintf(buffer, "\nMASTER MEM_FAULT: page fault not enough memory frame swap executed at time %d.%d\n\n", system_clock->sec, system_clock->ns);
					print_and_write(buffer);
					memset(buffer, 0, sizeof(buffer));
				}

				//Send a message to child process whether if it safe to proceed the request OR not
				master_msg.mtype = pcb[q_id].actual_pid;
				//Signal that request is granted
				msgsnd(msg_q_id, &master_msg, (sizeof(struct Message) - sizeof(long)), 0);
			
				//Trying to synchronized
				msgrcv(msg_q_id, &master_msg, (sizeof(struct Message) - sizeof(long)), 1, 0);
			} //End of Request flag

			//Increase iterration
			current_iteration++;			

			//Point the pointer to the next queue element
			next.next = (next.next->next != NULL) ? next.next->next : NULL;
		} //End of queue traversal
		free(t_queue);

		sem_lock(0);
		incr_clock(system_clock, random_time_elapsed());
		sem_release(0);

		//--------------------------------------------------
		//Check to see if a child exit, wait no bound (return immediately if no child has exit)
		int child_status = 0;
		pid_t finish_pid = waitpid(-1, &child_status, WNOHANG);

		//Set the return index bit back to zero (which mean there is a spot open for this specific index in the bitmap)
		if(finish_pid > 0)
		{
			int j;
			for(j=0; j<MAX_PROCESS; j++){
				if(child_pids[j] == finish_pid){
					child_pids[j] = 0;
					break;
				}
			}
			int return_index = WEXITSTATUS(child_status);
			bit_map[return_index / 8] &= ~(1 << (return_index % 8));
			int b;
			for(b = 0; b < PAGE_TABLE_SIZE; b++){
				int frame_numb = pcb[return_index].pg_tbl[b].address;
				main_memory[frame_numb / 8] &= ~(1 << (frame_numb % 8));
			}
		}

	} //End of Main Loop

	if(!already_clean){	//If cleaned up after fork failed dont clean again
		cleanup_and_exit();
	}

	return 0;
}

void print_and_write(char* str) {
    fputs(str, stdout);
    fputs(str, fptr);
}

/*****************************************
	Print statistics for program run.
*****************************************/
void print_statistics() {
    char buffer[2000];

    sprintf(buffer, "<<<Statistics>>>\n");
    sprintf(buffer + strlen(buffer), "  %-22s: %'d\n", "Total Processes", total_process);
    sprintf(buffer + strlen(buffer), "  %-22s: %'d\n", "Total Granted Requests", granted);
    sprintf(buffer + strlen(buffer), "  %-22s: %'d\n", "Total Requests", total_requests);
    sprintf(buffer + strlen(buffer), "  %-22s: %'d\n", "Total FIFO ran", total_fifo);
    sprintf(buffer + strlen(buffer), "  %-22s: %'d\n", "Total LRU ran", total_lru);
    sprintf(buffer + strlen(buffer), "  %-22s: %'d\n", "Total Page Faults", page_faults);

    sprintf(buffer + strlen(buffer), "\n");
    
    print_and_write(buffer);
}
/**************************************
	Pops the first element in a linked
	lis than clear out that element
	from being in main memory
**************************************/
int clear_and_pop(){
	struct LNode* fr;
	if(fifo_or_lru){
		fr = fifo_pop(&fifo_head);
	}
	else{
		fr = fifo_pop(&lru_head);
	}
	pcb[fr->pid].pg_tbl[fr->page_numb].address = 0;
	pcb[fr->pid].pg_tbl[fr->page_numb].valid = 0;
	pcb[fr->pid].pg_tbl[fr->page_numb].protn = 0;
	return fr->frame;
}
/**************************************
	Find a free frame in main memory
**************************************/
int get_free_frame(){
	int proc_count = 0;
	//bool is_bit_open = false;
	int frame_numb = -1;
    while(1){
        frame_numb = (frame_numb+1) % (MAIN_MEMORY_SIZE*8);
        uint32_t bit = main_memory[frame_numb / 8] & (1 << (frame_numb % 8));
        if(bit == 0){
			main_memory[frame_numb / 8] |= (1 << (frame_numb % 8));
		//	fprintf(stderr, "--MASTER MAIN MEMEORY SLOT [%d]\n", frame_numb);
            return frame_numb;
        }
        
        if(proc_count >= (MAIN_MEMORY_SIZE*8)){
        //    fprintf(stderr, "++OSS: frame counter:[%d])\n", frame_numb);
            return -1;
        }
        proc_count++;
    } //End of bit_map
	
}

/**************************************
	Initialize process control block
	to empty stage
**************************************/
void __init_pcb_start(struct process_control_block *pcb)
{
	int i;
	for(i = 0; i < MAX_PROCESS; i++)
	{
		pcb[i].pid = -1;
		pcb[i].actual_pid = -1;
	}		
}
/**************************************
	Populate pcb with created child
	information
**************************************/
void __init_pcb(struct process_control_block *pcb, int id, pid_t pid)
{
	pcb->pid = id;
	pcb->actual_pid = pid;
	
	int i;
	for(i = 0; i < PAGE_TABLE_SIZE; i++)
	{
		pcb->pg_tbl[i].address = 0;
		pcb->pg_tbl[i].protn = rand()%2? 1 : 0;
		pcb->pg_tbl[i].dirty = 0;
		pcb->pg_tbl[i].valid = 0;
	}
}
// ============================================================================================= //
// =================================================  Clock methods  ================================= //
void incr_clock(struct Clock* Clock, int elapsed_sec) {
    Clock->ns += elapsed_sec;
    if (Clock->ns >= ONE_BILLION) {
        Clock->sec += 1;
        Clock->ns -= ONE_BILLION;
    }
}

struct Clock add_clocks(struct Clock clock_1, struct Clock clock_2) {
    struct Clock out = {
        .sec = 0,
        .ns = 0
    };
    out.sec = clock_1.sec + clock_2.sec;
    incr_clock(&out, clock_1.ns + clock_2.ns);
    return out;
}

int compare_clocks(struct Clock clock_1, struct Clock clock_2) {
    if (clock_1.sec > clock_2.sec) {
        return 1;
    }
    if ((clock_1.sec == clock_2.sec) && (clock_1.ns > clock_2.ns)) {
        return 1;
    }
    if ((clock_1.sec == clock_2.sec) && (clock_1.ns == clock_2.ns)) {
        return 0;
    }
    return -1;
}

long double Clock_to_sec(struct Clock c) {
    long double sec = c.sec;
    long double ns = (long double)c.ns / ONE_BILLION; 
    sec += ns;
    return sec;
}

struct Clock sec_to_Clock(long double sec) {
    struct Clock clk = { .sec = (int)sec };
    sec -= clk.sec;
    clk.ns = sec * ONE_BILLION;
    return clk;
}

struct Clock calculate_avg_time(struct Clock clk, int divisor) {
    long double sec = Clock_to_sec(clk);
    long double avg_sec = sec / divisor;
    return sec_to_Clock(avg_sec);
}

struct Clock subtract_Clocks(struct Clock clock_1, struct Clock clock_2) {
    long double sec1 = Clock_to_sec(clock_1);
    long double sec2 = Clock_to_sec(clock_2);
    long double result = sec1 - sec2;
    return sec_to_Clock(result);
}

struct Clock ns_to_Clock(int ns) {
    struct Clock clk = { 
        .sec = 0, 
        .ns = 0 
    };

    if (ns >= ONE_BILLION) {
        ns -= ONE_BILLION;
        clk.sec = 1;
    }

    clk.ns = ns;
    
    return clk;
}

struct Clock get_clock() {
    struct Clock out = {
        .sec = 0,
        .ns = 0
    };
    return out;
}

void set_clock(struct Clock* clk) {
    clk->sec = 0;
    clk->ns = 0;
}

/**************************************
	Random elapsed time for simulated
	clock
**************************************/
unsigned int random_time_elapsed() {
    return (rand() % 500000) + 100000; 
}

/******************************************
	Used to set up time for next process
******************************************/
struct Clock get_fork_time_new_proc(struct Clock system_clock) {
	
    unsigned int time_before_next_process = rand() % FIVE_HUNDRED_MS; 
    incr_clock(&system_clock, time_before_next_process);	//Add random need to elapse time to current time and set to forkable time
    return system_clock;
	
}

// ============================================================================================= //

// =====================================  Clean up and exit  ==================================== //

/***************************************
	Terminate all existing processes
***************************************/
void terminate_children() {
	fprintf(stderr, "\nTerminate sequence initiated..\n");
	int id;
	for(id = 0; id < MAX_PROCESS; id++){
		if(child_pids[id] != 0){
			if(kill(child_pids[id], 0) == 0){
				fprintf(stdout, "TERMINATE: PID(%d)\n", child_pids[id]);
				if(kill(child_pids[id], SIGTERM) != 0){
					perror("Child can't be terminated for unkown reason\n");
				}
			}
		}
	}
}

/**************************************
	Clean up everything and terminate
**************************************/
void cleanup_and_exit() {
    terminate_children();
    fprintf(stdout, "OSS: Removing message queues and shared memory\n");
    fprintf(fptr, "OSS: Removing message queues and shared memory\n");
    remove_message_queue(msg_q_id);
    cleanup_shared_memory(clock_shmid, system_clock);
    cleanup_shared_memory(pcb_shmid, pcb);
	if(semid > 0)
	{
		semctl(semid, 0, IPC_RMID);
	}
	print_statistics();
    fclose(fptr);
}

// ===============================================   Interrupt handler   =============================== //

/*************** 
* Set up timer *
***************/
static void setuptimer(int time){
	
	struct itimerval value;
	value.it_value.tv_sec = time;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	
	if(setitimer(ITIMER_REAL, &value, NULL) == -1)
	{
		perror("ERROR");
	}
}
 
/*******************
* Set up interrupt *
*******************/
static void setupinterrupt(){
	
	struct sigaction sa1;
	sigemptyset(&sa1.sa_mask);
	sa1.sa_handler = &myhandler;
	sa1.sa_flags = SA_RESTART;
	if(sigaction(SIGALRM, &sa1, NULL) == -1)
	{
		perror("ERROR");
	}

	//Signal Handling for: SIGINT
	struct sigaction sa2;
	sigemptyset(&sa2.sa_mask);
	sa2.sa_handler = &myhandler;
	sa2.sa_flags = SA_RESTART;
	if(sigaction(SIGINT, &sa2, NULL) == -1)
	{
		perror("ERROR");
	}

	//Signal Handling for: SIGUSR1
	signal(SIGUSR1, SIG_IGN);	
}

/************************
* Set up my own handler *
************************/
static void myhandler(int s){
	
	fprintf(stdout, "\n!!!Termination begin since timer reached its time!!!\n");
	cleanup_and_exit();
	exit(0);

}

// =========================================   Semaphore set up   ======================================= //

/**************************************
	Used for locking clock while inc-
	rementing as critical section
**************************************/
void sem_lock(int sem_index)
{
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = -1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}

/**************************************
	Releases clock after modifying it
**************************************/
void sem_release(int sem_index)
{	
	sema_operation.sem_num = sem_index;
	sema_operation.sem_op = 1;
	sema_operation.sem_flg = 0;
	semop(semid, &sema_operation, 1);
}
