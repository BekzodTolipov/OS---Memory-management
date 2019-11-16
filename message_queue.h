#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H
#include <stdbool.h>    //bool variable

#define SIZE 2024

struct Message{
    long mtype;    
	int pid;
    pid_t actual_pid;
    int flag;   //0 : isDone | 1 : isQueue
    bool is_request;
    bool is_release;
	bool granted;
	bool read_or_write;
	unsigned int page_number : 15;
    //bool isSafe;
    char mtext[SIZE];
};


int get_message_queue(key_t key);
void remove_message_queue(int msg_queue_id );
void receive_msg(int msg_queue_id , struct Message* mbuf, int mtype);
void receive_msg_no_wait(int msg_queue_id , struct Message* mbuf, int mtype);
void send_msg(int msg_queue_id , struct Message* mbuf, int mtype);

#endif
