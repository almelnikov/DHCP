#include "shared.h"

struct sembuf sem_unlock = {0, -1, 0};
struct sembuf sem_lock[2] = {{0, 0, 0}, {0, 1, 0}};

void binsem_unlock(int semid, unsigned int n)
{
    struct sembuf sem = sem_unlock;

    sem.sem_num = n;
    semop(semid, &sem, 1);
}

void binsem_lock(int semid, unsigned int n)
{
    
    struct sembuf sem[2];

    sem[0] = sem_lock[0];
    sem[1] = sem_lock[1];
    sem[0].sem_num = n;
    sem[1].sem_num = n;
    semop(semid, sem, 2);
}