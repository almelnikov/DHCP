#ifndef __SHARED_H
#define __SHARED_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define NICK_SIZE 64
#define MSG_SIZE 256
#define MAX_CLIENTS 20

#define CHAT_MSG 0
#define CHAT_EXIT 1
#define CHAT_NICK 2

#define SEM_NEWNICK (MAX_CLIENTS * 4)
#define SEM_NEWID (MAX_CLIENTS * 4 + 1)
#define SEM_MEMNICK (MAX_CLIENTS * 4 + 2)
#define SEM_MEMID (MAX_CLIENTS * 4 + 3)
// Total nimber of semaphores
#define SEM_NUM (MAX_CLIENTS * 4 + 4)

#define SEM_OUT_MEM 0
#define SEM_OUT_READY MAX_CLIENTS
#define SEM_IN_MEM (MAX_CLIENTS * 2)
#define SEM_IN_READY (MAX_CLIENTS * 3)

struct user_io_t {
    char nick[NICK_SIZE];
    char msg_in[MSG_SIZE], msg_out[MSG_SIZE];
    int op_in, op_out;
};

struct chat_struct {
    char newnick[NICK_SIZE];
    int newid;
    struct user_io_t users[MAX_CLIENTS];
};

void binsem_lock(int, unsigned int);
void binsem_unlock(int, unsigned int);

#endif