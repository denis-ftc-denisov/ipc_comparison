#ifndef SYNCHRONOUS_SOCKET_HEADER_FILE_INCLUDED
#define SYNCHRONOUS_SOCKET_HEADER_FILE_INCLUDED

#include <string>

using namespace std;

class SynchronousSocket
{
private:
	const int TIMEOUT = 10; // in seconds
	const int BUFFER_SIZE = 1048576;
protected:
	// собственно сокет
	int socket;
public:
    SynchronousSocket();
	// Используется для проброса уже готового сокета и работы с ним
	void Init(int s);
	bool IsConnected();
	virtual void Connect(string host, int port);
	virtual void Send(const string &data);
	virtual string Receive(int amount);
	void Disconnect();
	string ReceiveMessage();
	void SendMessage(const string &m);
};


#endif
