#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#define SIZE 2024

struct Message {
    long mtype;
    char mtext[SIZE];
};

int get_message_queue(key_t key);
void remove_message_queue(int msg_queue_id );
void receive_msg(int msg_queue_id , struct Message* mbuf, int mtype);
void receive_msg_no_wait(int msg_queue_id , struct Message* mbuf, int mtype);
void send_msg(int msg_queue_id , struct Message* mbuf, int mtype);

#endif
