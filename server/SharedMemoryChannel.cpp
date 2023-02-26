#include "SharedMemoryChannel.h"
#include "mutex.h"

#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/file.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <dirent.h>
#include <sstream>
#include <iostream>
#include <vector>

using namespace std;

bool operator<(const SharedMemoryChannelID &a, const SharedMemoryChannelID &b)
{
    if (a.reuse_at < b.reuse_at) return true;
    if (a.reuse_at > b.reuse_at) return false;
    return a.internal_id < b.internal_id;
}

SharedMemoryChannel::SharedMemoryChannel()
{
	semaphore_send = semaphore_receive = nullptr;
	shared_mem = -1;
	shared = nullptr;
	id = id_provider.GetID();
    cerr << "SharedMemoryChannel::GetID " << id.id << "\n";
	{
		ostringstream oss;
        oss << "/smc_send_" << id.id;
		semaphore_send_key = oss.str();
	}
	{
		ostringstream oss;
        oss << "/smc_receive" << id.id;
		semaphore_receive_key = oss.str();
	}
	char buf[2048];
	memset(buf, 0, sizeof(buf));
    sprintf(buf, "/tmp/smc_%s", id.id.c_str());
	auto t = fopen(buf, "w");
	if (t == nullptr)
	{
		id_provider.ReleaseID(id);
		throw (string)("Unable to create memory channel IPC file");
	}
	fclose(t);
    sem_unlink(semaphore_receive_key.c_str());
	semaphore_receive = sem_open(semaphore_receive_key.c_str(), O_CREAT, 0666, 1);
	if (semaphore_receive == SEM_FAILED)
	{
		id_provider.ReleaseID(id);
		ostringstream oss;
		oss << "Unable to init receive semaphore " << errno << " key = " << semaphore_receive_key;
		throw oss.str();
	}
	sem_unlink(semaphore_send_key.c_str());
	semaphore_send = sem_open(semaphore_send_key.c_str(), O_CREAT, 0666, 1);
	if (semaphore_send == SEM_FAILED)
	{
		id_provider.ReleaseID(id);
		ostringstream oss;
		oss << "Unable to init send semaphore " << errno << " key = " << semaphore_send_key;
		throw oss.str();
	}
	// now create shared memory block
	shared_mem_key = ftok(buf, 2);
	shared_mem = shmget(shared_mem_key, MAX_LEN + 4, IPC_CREAT | 0777);
	if (shared_mem == -1)
	{
		if (semaphore_receive != nullptr)
		{
			sem_close(semaphore_receive);
		}
		if (semaphore_send != nullptr)
		{
			sem_close(semaphore_send);
		}
		id_provider.ReleaseID(id);
		ostringstream oss;
		oss << "Unable to get shared mem segment, error = " << errno;
		throw oss.str();
	}
	shared = shmat(shared_mem, nullptr, 0);
	if (shared == (void*)-1)
	{
		if (semaphore_receive != nullptr)
		{
			sem_close(semaphore_receive);
		}
		if (semaphore_send != nullptr)
		{
			sem_close(semaphore_send);
		}
		id_provider.ReleaseID(id);
		throw (string)("Failed to attach to shared memory");
	}
	// acquire both semaphores
	// our receive semaphore will be released by client
	try {
		AcquireSemaphore(semaphore_send);
		AcquireSemaphore(semaphore_receive);
	}
	catch (...){
		shmctl(shared_mem, IPC_RMID, nullptr);
		throw;
	}
}

SharedMemoryChannel::~SharedMemoryChannel()
{
    cerr << "SharedMemoryChannel::ReleaseID " << id.id << "\n";
    if (semaphore_receive != nullptr)
	{
		sem_close(semaphore_receive);
	}
	if (semaphore_send != nullptr)
	{
		sem_close(semaphore_send);
	}
	if (shared_mem != -1)
	{
		if (shared != nullptr)
		{
			shmdt(shared);
		}
		shmctl(shared_mem, IPC_RMID, nullptr);
	}
    id_provider.ReleaseID(id);
}

string SharedMemoryChannel::GetSendKey() const
{
	return semaphore_send_key;
}

string SharedMemoryChannel::GetReceiveKey() const
{
	return semaphore_receive_key;
}

key_t SharedMemoryChannel::GetSharedMemKey() const
{
	return shared_mem_key;
}

void SharedMemoryChannel::AcquireSemaphore(sem_t* semaphore)
{
	struct timespec timeout;
    auto start = time(NULL);
    while (true)
	{
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += TIMEOUT_MSEC / 1000;
        timeout.tv_nsec += (TIMEOUT_MSEC % 1000) * 1000000;
        while (timeout.tv_nsec >= 1000000000)
        {
            timeout.tv_sec++;
            timeout.tv_nsec -= 1000000000;
        }
        if (sem_timedwait(semaphore, &timeout) == -1)
		{
			if (errno != EAGAIN && errno != ETIMEDOUT)
			{
				ostringstream oss;
				oss << "AcquireSemaphore failed " << errno;
				throw oss.str();
			}
		}
		else
		{
			// semaphore operation went well
			return;
		}
        // check timeout
        if (time(NULL) - start > CHANNEL_CLEAR_TIMEOUT_SEC)
        {
            throw (string)("Channel inactive, remove it");
        }
	}
}

void SharedMemoryChannel::ReleaseSemaphore(sem_t* semaphore)
{
	if (sem_post(semaphore) == -1)
	{
		ostringstream oss;
		oss << "ReleaseSemaphore failed " << errno;
		throw oss.str();
	}
}

std::string SharedMemoryChannel::Receive()
{
	AcquireSemaphore(semaphore_receive);
	char *sc = (char*)shared;
	int len = 0;
	for (int i = 3; i >= 0; i--)
	{
		len = (len << 8) | ((int)(unsigned char)sc[i]);
	}
	return string(&sc[4], len);
}

void SharedMemoryChannel::Send(const string &s)
{
	if (s.length() > MAX_LEN) throw (string)("Too long message to pass through shared mem channel");
	char *sc = (char*)shared;
	int len = s.length();
	for (int i = 0; i < 4; i++)
	{
		sc[i] = len & 0xFF;
		len >>= 8;
	}
	memcpy(&sc[4], s.c_str(), s.length());
	ReleaseSemaphore(semaphore_send);
}

SharedMemoryChannelIDProvider::SharedMemoryChannelIDProvider()
{
	if (pthread_mutex_init(&access_mutex, NULL) != 0)
	{
		throw (string)("Unable to create access mutex");
	}
    FILE *f = fopen("id", "r");
    string id = "";
    if (f == nullptr)
    {
        for (int i = 0; i < 30; i++)
        {
            id += (char)(rand() % 26 + 'A');
        }
    }
    else
    {
        int c = fgetc(f);
        while (c != -1)
        {
            if (c >= 'A' && c <= 'Z')
            {
                id += (char)c;
            }
            c = fgetc(f);
        }
        fclose(f);
    }

	// clear all possible semaphores (in order to make sure that everyone
	// waiting for them will be woken up)
    DIR* dir = opendir("/dev/shm");
    if (!dir)
    {
        throw (string)("Could not open semaphores directory");
    }
    struct dirent cdir;
    struct dirent *p;
    if (readdir_r(dir, &cdir, &p))
    {
        throw (string)("Error during reading semaphores directory");
    }
    string to_check = "smc" + id;
    vector<string> to_unlink;
    while (p)
    {
        string cname(cdir.d_name);
        if (cname.substr(0, to_check.length()) == to_check)
        {
            to_unlink.push_back("/dev/shm/" + cname);
        }
        if (readdir_r(dir, &cdir, &p))
        {
            throw (string)("Error during reading semaphores directory");
        }
    }
    closedir(dir);
    for (int i = 0; i < (int)to_unlink.size(); i++)
    {
        unlink(to_unlink[i].c_str());
    }
}

SharedMemoryChannelIDProvider SharedMemoryChannel::id_provider;

SharedMemoryChannelID SharedMemoryChannelIDProvider::GetID()
{
    LockMutex(access_mutex);
    if (used_ids.size() > 0)
    {
        auto t = used_ids.begin();
        if (t->reuse_at <= time(nullptr))
        {
            auto result = *t;
            used_ids.erase(t);
            ReleaseMutex(access_mutex);
            return result;
        }
    }
    int id = last_id++;
    ReleaseMutex(access_mutex);
    ostringstream oss;
    oss << prefix << "_" << id;
    SharedMemoryChannelID result;
    result.internal_id = id;
    result.id = oss.str();
    return result;
}

void SharedMemoryChannelIDProvider::ReleaseID(const SharedMemoryChannelID &id)
{
    LockMutex(access_mutex);
    auto t = id;
    t.reuse_at = time(nullptr) + ID_REUSE_TIMEOUT;
    used_ids.insert(t);
    ReleaseMutex(access_mutex);
}
