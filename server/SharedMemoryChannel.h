#ifndef SHAREDMEMORYCHANNEL_H
#define SHAREDMEMORYCHANNEL_H

#include <string>
#include <sys/types.h>
#include <sys/ipc.h>
#include <set>
#include <pthread.h>
#include <semaphore.h>

class SharedMemoryChannelID
{
public:
    time_t reuse_at;
    int internal_id;
    std::string id;
};

bool operator<(const SharedMemoryChannelID &a, const SharedMemoryChannelID &b);

class SharedMemoryChannelIDProvider
{
    // Таймаут, после которого считается, что id канала безопасно переиспользовать.
    // В секундах.
    const int ID_REUSE_TIMEOUT = 60;

    int last_id;
	pthread_mutex_t access_mutex;
    std::string prefix;

    std::set<SharedMemoryChannelID> used_ids;
public:
	SharedMemoryChannelIDProvider();
    SharedMemoryChannelID GetID();
    void ReleaseID(const SharedMemoryChannelID &id);
};

/*
 * Обмен с PHP-клиентом через shared memory.
 * Управляется двумя семафорами. Общий цикл выглядит так:
 * 1) Оба семафора залочены, мы ждем semaphore_receive.
 * 2) Клиент кладет в shared mem сообщение, разлочивает semaphore_receive.
 * 3) Мы получаем semaphore_receive, читаем сообщение. Клиент ждет semaphore_send.
 * 4) Разлочим semaphore_send, клиент лочит semaphore_send и получает сообщение,
 *
 * Итого общий принцип - при получении сообщения получатель
 * лочит семафор, ответственный за передачу в его сторону. И
 * не разлочивает его, задача разлочивать - на отправителе.
 * Отправитель соответственно когда готова отправка разлочивает
 * семафор передачи "от него".
*/
class SharedMemoryChannel
{
    const static int TIMEOUT_MSEC = 100;
    // через вот такое количество времени считаем, что канал уже не используется и его можно прибивать
    const static int CHANNEL_CLEAR_TIMEOUT_SEC = 10;
	// Храним использованные id-шники
	// с тем, чтобы разные каналы использовали разные
	// id, но при этом набор id-шников оставался
	// бы неизменным при повторных запусках.
	// Это чинит неприятную проблему, связанную с
	// прибиванием процесса, который пользуется семафорами.
	static SharedMemoryChannelIDProvider id_provider;
	// 4 bytes for length
	key_t shared_mem_key;
	int shared_mem;
	void *shared;
	std::string semaphore_send_key;
	sem_t* semaphore_send;
	std::string semaphore_receive_key;
	sem_t* semaphore_receive;
	//
	void AcquireSemaphore(sem_t *semaphore);
	void ReleaseSemaphore(sem_t *semaphore);
	//
    SharedMemoryChannelID id;
public:
	const static int MAX_LEN = 1048076 - 4;
	std::string GetSendKey() const;
	std::string GetReceiveKey() const;
	key_t GetSharedMemKey() const;
	SharedMemoryChannel();
	~SharedMemoryChannel();
	std::string Receive();
	void Send(const std::string &s);
};

#endif // SHAREDMEMORYCHANNEL_H
