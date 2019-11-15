#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "message_queue.h"

int get_message_queue(key_t key) {
    int msg_queue_id ;

    msg_queue_id  = msgget(key, IPC_CREAT | 0666);

    if (msg_queue_id  == -1) {
        perror("msgget");
        exit(1);
    }

    return msg_queue_id ;
}

void receive_msg(int msg_queue_id , struct Message* msg, int mtype) {
    if (msgrcv(msg_queue_id , msg, sizeof(msg->mtext), mtype, 0) == -1) {
        perror("msgrcv");
        exit(1);
    }
}

void receive_msg_no_wait(int msg_queue_id , struct Message* msg, int mtype) {
    sprintf(msg->mtext, "0");
    if (msgrcv(msg_queue_id , msg, sizeof(msg->mtext), mtype, IPC_NOWAIT) == -1) {
        if (errno == ENOMSG) {
            // No message of type mtype
            return;
        }
        perror("msgrcv");
        exit(1);
    }
}

void send_msg(int msg_queue_id , struct Message* msg, int mtype) {
    msg->mtype = mtype;
    if (msgsnd(msg_queue_id , msg, sizeof(msg->mtext), IPC_NOWAIT) < 0) {
        perror("msgsnd");
        exit(1);
    }
}

void remove_message_queue(int msg_queue_id ) {
    if (msgctl(msg_queue_id , IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }
}
