#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

void LockMutex(pthread_mutex_t &m);
void ReleaseMutex(pthread_mutex_t &m);

#endif
