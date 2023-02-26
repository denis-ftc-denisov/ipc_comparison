#include <string>

#include "mutex.h"

using namespace std;

void LockMutex(pthread_mutex_t &m)
{
	int mres;
	while ((mres = pthread_mutex_lock(&m)) != 0)
	{
		if (mres != EAGAIN)
		{
			throw (string)("Fatal error acquiring mutex");
		}
	}
}

void ReleaseMutex(pthread_mutex_t &m)
{
	int mres = pthread_mutex_unlock(&m);
	if (mres != 0)
	{
		throw (string)("Error releasing mutex");
	}
}
