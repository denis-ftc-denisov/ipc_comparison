#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/file.h>
#include <errno.h>
#include <iostream>
#include <sys/un.h>
#include <sstream>
#include <signal.h>

#include "SharedMemoryChannel.h"
#include "SynchronousSocket.h"
#include "task.h"

using namespace std;

const int LISTEN_PORT = 19876;
const int LISTEN_BACKLOG = 10;
const int EXIT_FAIL = 1;

const char* UNIX_SOCKET_PATH = "/tmp/ipc_test_socket";
const char* SHMEM_SOCKET_PATH = "/tmp/ipc_test_socket_shmem";

void CreateThread(void *(*start)(void *), void* arg = nullptr)
{   
	pthread_t thread;
	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0)
	{                    	
		throw (string)("pthread_attr_init error");
	}
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED))
	{
		throw (string)("pthread_attr_setdetachstate error");
	}                 
	if (pthread_create(&thread, &attr, start, arg) != 0)
	{                      
		throw (string)("pthread_create error");
	}                
}

void* ProcessRequests(void *arg)
{
	int socket_fd = *(int*)arg;
	SynchronousSocket s;
	s.Init(socket_fd);
	try {
		while (true)
		{
			string message = s.ReceiveMessage();
			s.SendMessage(Task(message));
		}
	}
	catch (string s)
	{
		cerr << "Error in socket " << s << "\n";
	}
	catch (...)
	{
		cerr << "Unknown error in socket\n";
	}
	s.Disconnect();
}

void* IPSocketThread(void* arg)
{
	int listensock;
	if ((listensock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		cerr << "Could not create socket for listening...\n";
		exit(EXIT_FAIL);
	}
	struct sockaddr_in listenaddr;
	memset(&listenaddr, 0, sizeof(listenaddr));
	listenaddr.sin_family = AF_INET;
	listenaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	listenaddr.sin_port = htons(LISTEN_PORT);
	if (bind(listensock, (struct  sockaddr *)(&listenaddr), sizeof(struct sockaddr_in)) == -1)
	{
		cerr << "Could not bind listening socket...\n";
		exit(EXIT_FAIL);
	}
	if (listen(listensock, LISTEN_BACKLOG) == -1)
	{
		cerr << "Could not set socket into listening mode...\n";
		exit(EXIT_FAIL);
	}
	struct sockaddr_in peeraddr;
	socklen_t paddrlen;
	paddrlen = sizeof(peeraddr);
	int recsock;
	cout << "Accepting connections on port " << LISTEN_PORT << "\n";           
	while (true)
	{
		(recsock = accept(listensock, (struct sockaddr *)(&peeraddr), &paddrlen));
		if (recsock == -1)
		{
			cerr << "Error accepting connection, errno = " << errno << "\n";
			continue;
		}
		cout << "Accepted connection from " << inet_ntoa(peeraddr.sin_addr) << ":" << ntohs(peeraddr.sin_port) << "\n";
		CreateThread(&ProcessRequests, &recsock);
	}                                                   
	cerr << "Accept error...\n";
	if (close(listensock) == -1)
	{
		cerr << "Could not close listening socket...\n";
		exit(EXIT_FAIL);
	}
}

void* UnixSocketThread(void* arg)
{
	int listensock;
	if ((listensock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	{
		cerr << "Could not create socket for listening...\n";
		exit(EXIT_FAIL);
	}
	struct sockaddr_un listenaddr;
	listenaddr.sun_family = AF_UNIX;
	strcpy(listenaddr.sun_path, UNIX_SOCKET_PATH);
	unlink(listenaddr.sun_path);
	socklen_t len = strlen(listenaddr.sun_path) + sizeof(listenaddr.sun_family);
	if (bind(listensock, (struct sockaddr*)&listenaddr, len) != 0)
	{
		cerr << "Error on binding socket \n";
		perror("bind");
		exit(EXIT_FAIL);
	}
	if (listen(listensock, LISTEN_BACKLOG) == -1)
	{
		cerr << "Could not set socket into listening mode...\n";
		exit(EXIT_FAIL);
	}
	struct sockaddr_un peeraddr;
	socklen_t paddrlen;
	paddrlen = sizeof(peeraddr);
	int recsock;
	cout << "Accepting connections on path " << UNIX_SOCKET_PATH << "\n";           
	while (true)
	{
		(recsock = accept(listensock, (struct sockaddr *)(&peeraddr), &paddrlen));
		if (recsock == -1)
		{
			cerr << "Error accepting connection, errno = " << errno << "\n";
			continue;
		}
		cout << "Accepted connection from " << peeraddr.sun_path << "\n";
		CreateThread(&ProcessRequests, &recsock);
	}                                                   
	cerr << "Accept error...\n";
	if (close(listensock) == -1)
	{
		cerr << "Could not close listening socket...\n";
		exit(EXIT_FAIL);
	}
}

void* ProcessShmemRequests(void *arg)
{
	int socket_fd = *(int*)arg;
	SynchronousSocket s;
	s.Init(socket_fd);
	try {
		string initial = s.ReceiveMessage();
		SharedMemoryChannel channel;
		ostringstream result;
		result << channel.GetReceiveKey() << ":" << 
						channel.GetSendKey() << ":" <<
						channel.GetSharedMemKey();
		s.SendMessage(result.str());
		while (true)
		{
			string message = channel.Receive();
			channel.Send(Task(message));
		}
	}
	catch (string s)
	{
		cerr << "Error in socket " << s << "\n";
	}
	catch (...)
	{
		cerr << "Unknown error in socket\n";
	}
	s.Disconnect();                                 
}

void* SharedMemoryChannelSocketThread(void* arg)
{
	int listensock;
	if ((listensock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	{
		cerr << "Could not create socket for listening...\n";
		exit(EXIT_FAIL);
	}
	struct sockaddr_un listenaddr;
	listenaddr.sun_family = AF_UNIX;
	strcpy(listenaddr.sun_path, SHMEM_SOCKET_PATH);
	unlink(listenaddr.sun_path);
	socklen_t len = strlen(listenaddr.sun_path) + sizeof(listenaddr.sun_family);
	if (bind(listensock, (struct sockaddr*)&listenaddr, len) != 0)
	{
		cerr << "Error on binding socket \n";
		perror("bind");
		exit(EXIT_FAIL);
	}
	if (listen(listensock, LISTEN_BACKLOG) == -1)
	{
		cerr << "Could not set socket into listening mode...\n";
		exit(EXIT_FAIL);
	}
	struct sockaddr_un peeraddr;
	socklen_t paddrlen;
	paddrlen = sizeof(peeraddr);
	int recsock;
	cout << "Accepting connections on path " << SHMEM_SOCKET_PATH << "\n";           
	while (true)
	{
		(recsock = accept(listensock, (struct sockaddr *)(&peeraddr), &paddrlen));
		if (recsock == -1)
		{
			cerr << "Error accepting connection, errno = " << errno << "\n";
			continue;
		}
		CreateThread(&ProcessShmemRequests, &recsock);
	}                                                   
	cerr << "Accept error...\n";
	if (close(listensock) == -1)
	{
		cerr << "Could not close listening socket...\n";
		exit(EXIT_FAIL);
	}
}

int main()
{
	signal(SIGPIPE, SIG_IGN);
    CreateThread(&IPSocketThread);
	CreateThread(&UnixSocketThread);
	CreateThread(&SharedMemoryChannelSocketThread);
	cout << "Press Ctrl-C for exit\n";
	while (true)
	{
		sleep(10);
	}
	return 0;
}
