#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <pthread.h>
#include "shared.h"

struct thread_info {
	int id, sem_id;
	struct chat_struct *chatptr;
} thr_data;

char textbuf[100000];
int exit_flag = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t input_thr;

struct chat_struct *chat_shm;

void remove_newline(char *str)
{
    int len;

    len = strlen(str);
    if (len > 0) {
        if (str[len - 1] == '\n')
            str[len - 1] = '\0';
    }
}

void *input_thread(void *ptr)
{
    struct thread_info *s = (struct thread_info*)ptr;
    char msgstr[MSG_SIZE], nickstr[NICK_SIZE];
    int op;
    int client_id = s->id;
    int sem_id = s->sem_id;
    struct chat_struct *chat_shm = s->chatptr;

    while (1) {
	binsem_lock(sem_id, client_id + SEM_IN_READY);
	binsem_lock(sem_id, client_id + SEM_IN_MEM);
	strcpy(msgstr, chat_shm->users[client_id].msg_in);
	strcpy(nickstr, chat_shm->users[client_id].nick);
	op = chat_shm->users[client_id].op_in;
	binsem_unlock(sem_id, client_id + SEM_IN_MEM);
	if (op == CHAT_MSG) {
	    sprintf(textbuf, "%s: %s\n", nickstr, msgstr);
	}
	else if (op == CHAT_NICK) {
	    sprintf(textbuf, "New user: %s\n", nickstr);
	}
	else if (op == CHAT_EXIT) {
	    sprintf(textbuf, "User exited: %s\n", nickstr);
	}
	printf("%s", textbuf);
	textbuf[0] = '\0';
    }

    return NULL;
}


int main()
{
    key_t key;
    int mem_id, sem_id;
    int client_id;
    char msgstr[MSG_SIZE], nickstr[NICK_SIZE];
    
    key = ftok("server", 'a');
    mem_id = shmget(key, sizeof(struct chat_struct), 0);
    if (mem_id < 0) {
        fprintf(stderr, "Cannot allocate shared memory\n");
        exit(0);
    }
    chat_shm = (struct chat_struct*) shmat(mem_id, NULL, 0);
    sem_id = semget(key, SEM_NUM, IPC_CREAT);

    printf("Input your nick: ");
    fgets(nickstr, NICK_SIZE, stdin);
    remove_newline(nickstr);

    binsem_lock(sem_id, SEM_MEMNICK);
    strcpy(chat_shm->newnick, nickstr);
    binsem_unlock(sem_id, SEM_NEWNICK);
    binsem_unlock(sem_id, SEM_MEMNICK);

    binsem_lock(sem_id, SEM_NEWID);
    binsem_lock(sem_id, SEM_MEMID);
    client_id = chat_shm->newid;
    binsem_unlock(sem_id, SEM_MEMID);

    printf("ID = %d\n", client_id);

    thr_data.id = client_id;
    thr_data.sem_id = sem_id;
    thr_data.chatptr = chat_shm;
    pthread_create(&input_thr, NULL, input_thread, (void*)&thr_data);

    do {
	fgets(msgstr, MSG_SIZE, stdin);
	remove_newline(msgstr);
	
	binsem_lock(sem_id, client_id + SEM_OUT_MEM);
	strcpy(chat_shm->users[client_id].msg_out, msgstr);
	chat_shm->users[client_id].op_out = CHAT_MSG;
	binsem_unlock(sem_id, client_id + SEM_OUT_READY);
        binsem_unlock(sem_id, client_id + SEM_OUT_MEM);
    } while (strcmp(msgstr, "") != 0);

    /*
    binsem_lock(sem_id, client_id + SEM_OUT_MEM);
    chat_shm->users[client_id].op_out = CHAT_EXIT;
    binsem_unlock(sem_id, client_id + SEM_OUT_READY);
    binsem_unlock(sem_id, client_id + SEM_OUT_MEM);
    */
    printf("EXIT!\n");

    return 0;
}