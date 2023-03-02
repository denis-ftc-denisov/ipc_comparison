#include "SynchronousSocket.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring>
#include <unistd.h>
#include <errno.h>

SynchronousSocket::SynchronousSocket()
{
	socket = -1;
}

void SynchronousSocket::Init(int s)
{
	socket = s;
}

bool SynchronousSocket::IsConnected()
{
	return (socket != -1);
}

void SynchronousSocket::Connect(string host, int port)
{
	// уже подключены?
	if (socket != -1) return;

	struct sockaddr_in remoteaddr;

	struct addrinfo *addresses;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host.c_str(), NULL, NULL, &addresses) != 0)
	{
		throw (string)("Unable to getaddrinfo");
		return;
	}
	// копипастим себе самый первый адрес (в принципе, нам любой норм)
	memcpy(&remoteaddr, addresses[0].ai_addr, sizeof(remoteaddr));
	remoteaddr.sin_port = htons(port);
	freeaddrinfo(addresses);
	// создаем сокет
	if ((socket = ::socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
		throw (string)("Unable to create socket");
		return;
	}
	if (connect(socket, (struct  sockaddr *)(&remoteaddr), sizeof(struct sockaddr_in)) == -1)
	{
		if (errno != EINPROGRESS)
		{
			socket = -1;
			throw (string)("Unable to connect");
			return;
		}
	}
}

void SynchronousSocket::Send(const string & data)
{
	int pos = 0;
	time_t start = time(NULL);
	while (pos < (int)data.length())
	{
		int res = send(socket, &((data.data())[pos]), data.length() - pos, 0);
		if (res == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				if (time(NULL) - start > TIMEOUT)
				{
					throw (string)("Timeout");
				}
				continue;
			}
			else
			{
				// сокет поломался - закрываем его
				throw (string)("Socket closed");
			}
		}
		else
		{
			pos += res;
		}
	}
}

string SynchronousSocket::Receive(int amount)
{
	string r = "";
	time_t start = time(NULL);
	while ((int)r.length() < amount)
	{
		char buf[1024];
		int to_recv = min((int)sizeof(buf), amount - (int)r.length());
		int res = recv(socket, buf, to_recv, 0);
		if (res == 0)
		{
			// бросаем специфический exception, означающий, что сокет закрыт
			throw (string)("Socket closed");
		}
		else if (res == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				if (time(NULL) - start > TIMEOUT)
				{
					throw (string)("Timeout");
				}
				continue;
			}
			else
			{
				// что-то сломалось, надо закрыть сокет на всякий случай
				// бросаем специфический exception, означающий, что сокет закрыт
				throw (string)("Socket closed");
			}
		}
		else
		{
			r += string(buf, res);
		}
	}
	return r;
}

void SynchronousSocket::Disconnect()
{
	if (socket == -1) return;
	close(socket);
	socket = -1;
}

string SynchronousSocket::ReceiveMessage()
{
	string len_s = Receive(4);
	int len = 0;
	for (size_t i = 0; i < len_s.length(); i++)
	{
		len |= ((unsigned char)len_s[i]) << (8 * i);
	}
	return Receive(len);
}

void SynchronousSocket::SendMessage(const string & m)
{
	int len = m.length();
	string len_s = "";
	for (int i = 0; i < 4; i++)
	{
		len_s += (char)((len >> (8 * i)) & 0xFF);
	}
	Send(len_s);
	Send(m);
}


