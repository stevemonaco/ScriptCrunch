#include "stdafx.h"
#include "utility.h"
#include <string>

using namespace std;

string tcstostring(const TCHAR* str)
{
	string s;
#ifdef _UNICODE
	// Must convert to ascii
	int len = (int) _tcslen(str);
	char* newstr = new char[len+1];
	wcstombs(newstr, str, len);
	newstr[len] = '\0';
	s = newstr;
#else
	s = str;
#endif
	return s;
}