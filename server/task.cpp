#include "task.h"
#include <cstring>
#include <sstream>

using namespace std;

string Task(const string &message)
{                
	string result = "";
	int result_length = message.length() / 8;
	if (result_length < 1) result_length = 1;
	for (int i = 0; i < result_length; i++) result += '\x0';
	for (int i = 0; i < (int)message.length(); i++)
	{
		result[i % result_length] ^= message[i];
	}
	return result;
}
