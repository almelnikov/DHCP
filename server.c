#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include "shared.h"

struct user_t {
    int active;
    char nick[NICK_SIZE];
};

struct ringbuf_t {
    pthread_mutex_t lock;
    struct message_t *ptrs[256];
    int len, current;
};

struct message_t {
    int op;
    char msg[MSG_SIZE], nick[NICK_SIZE];
};

struct thread_info {
    int id;
    int sem_id;
};

struct chat_struct *chat_shm;

struct user_t users[MAX_CLIENTS];
pthread_mutex_t users_lock = PTHREAD_MUTEX_INITIALIZER;
struct ringbuf_t buffers[MAX_CLIENTS];
struct thread_info read_info[MAX_CLIENTS];
struct thread_info write_info[MAX_CLIENTS];
pthread_t read_thrs[MAX_CLIENTS], write_thrs[MAX_CLIENTS];

void ringbuf_init(struct ringbuf_t *ptr)
{
    ptr->len = 0;
    ptr->current = 0;
    pthread_mutex_init(&ptr->lock, NULL);
}

void ringbuf_push(struct ringbuf_t *buffer, struct message_t *data)
{
    int pos;

    pthread_mutex_lock(&buffer->lock);
    pos = (buffer->len + buffer->current) % 256;
    buffer->ptrs[pos] = data;
    buffer->len++;
    pthread_mutex_unlock(&buffer->lock);
}

void *ringbuf_pop(struct ringbuf_t *buffer)
{
    struct message_t *data;
    int pos;

    pthread_mutex_lock(&buffer->lock);
    if (buffer->len != 0) {
	data = buffer->ptrs[buffer->current];
	buffer->current++;
	buffer->len--;
    }
    else {
	data = NULL;
    }
    pthread_mutex_unlock(&buffer->lock);
    return data;
}


int user_add(const char *nick)
{
    int i;
    int id = -1;

    pthread_mutex_lock(&users_lock);
    for (i = 0; i < MAX_CLIENTS; i++) {
	if (users[i].active == 0) {
	    users[i].active = 1;
	    strcpy(users[i].nick, nick);
	    id = i;
	    break;
	}
    }
    pthread_mutex_unlock(&users_lock);
    return id;
}

void user_delete(int id)
{
    pthread_mutex_lock(&users_lock);
    users[id].active = 0;
    pthread_mutex_unlock(&users_lock);
}

void broadcast(int id, char *msg)
{
    int i;
    struct message_t *msgptr;

    pthread_mutex_lock(&users_lock);
    for (i = 0; i < MAX_CLIENTS; i++) {
	if (users[i].active == 1 && i != id) {
	    msgptr = (struct message_t*)malloc(sizeof(struct message_t));
	    printf("Memory allocated: %p\n", (void*)msgptr);
	    strcpy(msgptr->nick, users[id].nick);
	    strcpy(msgptr->msg, msg);
	    msgptr->op = CHAT_MSG;
	    ringbuf_push(&buffers[i], msgptr);
	}
    }
    pthread_mutex_unlock(&users_lock);
}

void *read_thread(void *ptr)
{
    struct thread_info *s = (struct thread_info*)ptr;
    int id = s->id;
    int sem_id = s->sem_id;
    int op;
    char msgstr[NICK_SIZE];

    for (;;) {
	binsem_lock(sem_id, id + SEM_OUT_READY);
	binsem_lock(sem_id, id + SEM_OUT_MEM);
	op = chat_shm->users[id].op_out;
	strcpy(msgstr, chat_shm->users[id].msg_out);
	binsem_unlock(sem_id, id + SEM_OUT_MEM);
	printf("READ: ID = %d OP = %d MSG = %s\n", id, op, msgstr);
	if (op == CHAT_MSG && (strcmp(msgstr, "") != 0)) {
	    broadcast(id, msgstr);
	}
	else {
	    user_delete(id);
	    printf("User ID=%d exited\n", id);
	}
    }
    return NULL;
}

void *write_thread(void *ptr)
{
    struct thread_info *s = (struct thread_info*)ptr;
    int id = s->id;
    int sem_id = s->sem_id;
    struct message_t *msgptr;

    for (;;) {
	while ((msgptr = ringbuf_pop(&buffers[id])) == NULL);

	printf("WRITE: OP = %d NICK = %s MSG = %s\n", msgptr->op, msgptr->nick,
	       msgptr->msg);

	binsem_lock(sem_id, id + SEM_IN_MEM);
	strcpy(chat_shm->users[id].nick, msgptr->nick);
	strcpy(chat_shm->users[id].msg_in, msgptr->msg);
	chat_shm->users[id].op_in = msgptr->op;
	binsem_unlock(sem_id, id + SEM_IN_READY);
	binsem_unlock(sem_id, id + SEM_IN_MEM);
	free(msgptr);
    }
    return NULL;
}

int main()
{
    key_t key;
    int mem_id, sem_id;
    int i;
    int newuser_id;
    char newnick[NICK_SIZE];
    
    key = ftok("server", 'a');
    mem_id = shmget(key, sizeof(struct chat_struct), IPC_CREAT | 0666);
    if (mem_id < 0) {
	fprintf(stderr, "Cannot allocate shared memory\n");
	exit(0);
    }
    chat_shm = (struct chat_struct*) shmat(mem_id, NULL, 0);

    sem_id = semget(key, SEM_NUM, IPC_CREAT | 0666);
    if (sem_id < 0) {
	fprintf(stderr, "Cannot create semaphores\n");
	shmctl(mem_id, IPC_RMID, NULL);
	exit(0);
    }

    // default sem values
    semctl(sem_id, SEM_MEMNICK, SETVAL, 0);
    semctl(sem_id, SEM_MEMID, SETVAL, 0);
    semctl(sem_id, SEM_NEWNICK, SETVAL, 1);
    semctl(sem_id, SEM_NEWID, SETVAL, 1);
    for (i = 0; i < MAX_CLIENTS; i++) {
        semctl(sem_id, i + SEM_OUT_MEM, SETVAL, 0);
        semctl(sem_id, i + SEM_IN_MEM, SETVAL, 0);
        semctl(sem_id, i + SEM_OUT_READY, SETVAL, 1);
        semctl(sem_id, i + SEM_IN_READY, SETVAL, 1);
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
	ringbuf_init(&buffers[i]);
	read_info[i].id = i;
	read_info[i].sem_id = sem_id;
	write_info[i].id = i;
	write_info[i].sem_id = sem_id;
	pthread_create(&read_thrs[i], NULL, read_thread, (void*)&read_info[i]);
	pthread_create(&write_thrs[i], NULL, write_thread, (void*)&write_info[i]);
    }

    for (;;) {
	binsem_lock(sem_id, SEM_NEWNICK);
	binsem_lock(sem_id, SEM_MEMNICK);
	strcpy(newnick, chat_shm->newnick);
	newuser_id = user_add(newnick);
	binsem_unlock(sem_id, SEM_MEMNICK);

	printf("New user ID = %d NICK = %s\n", newuser_id, newnick);

	binsem_lock(sem_id, SEM_MEMID);
	chat_shm->newid = newuser_id;
	binsem_unlock(sem_id, SEM_NEWID);
	binsem_unlock(sem_id, SEM_MEMID);
    }

    shmctl(mem_id, IPC_RMID, NULL);
    return 0;
}