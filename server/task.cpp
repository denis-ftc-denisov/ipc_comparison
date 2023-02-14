#include "task.h"
#include <cstring>
#include <sstream>

using namespace std;

int cnt[256];

string Task(const string &message)
{                
	memset(cnt, 0, sizeof(cnt));
	for (int i = 0; i < (int)message.length(); i++)
	{
		cnt[(int)(unsigned char)message[i]]++;
	}
	ostringstream result;
	for (int i = 0; i < 256; i++)
	{
		if (cnt[i] == 0) continue;
		result << i << ":" << cnt[i] << ";";
	}
	return result.str();
}
