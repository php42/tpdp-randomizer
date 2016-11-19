/*
	Copyright (C) 2016 php42

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define WIN32_LEAN_AND_MEAN
#define CP_SJIS 932
#include <Windows.h>
#include "textconvert.h"

std::wstring sjis_to_utf(const std::string& str)
{
	std::wstring ret;

    int len = MultiByteToWideChar(CP_SJIS, MB_PRECOMPOSED, str.c_str(), -1, NULL, 0);
    if(!len)
        return ret;

    wchar_t *buf = new wchar_t[len];
    if(!MultiByteToWideChar(CP_SJIS, MB_PRECOMPOSED, str.c_str(), -1, buf, len))
        return ret;

    ret = buf;
    delete[] buf;

	return ret;
}

std::wstring sjis_to_utf(const char *begin, const char *end)
{
	std::wstring ret;

    int len = MultiByteToWideChar(CP_SJIS, MB_PRECOMPOSED, begin, end - begin, NULL, 0);
    if(!len)
        return ret;

    wchar_t *buf = new wchar_t[len];
    if(!MultiByteToWideChar(CP_SJIS, MB_PRECOMPOSED, begin, end - begin, buf, len))
        return ret;

    ret.assign(buf, len);
    delete[] buf;

	return ret;
}

std::string utf_to_sjis(const std::wstring& str)
{
	std::string ret;

    int len = WideCharToMultiByte(CP_SJIS, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
    if(!len)
        return ret;

    char *buf = new char[len];
    if(!WideCharToMultiByte(CP_SJIS, 0, str.c_str(), -1, buf, len, NULL, NULL))
        return ret;

    ret = buf;
    delete[] buf;

	return ret;
}

std::wstring utf_widen(const std::string& str)
{
	std::wstring ret;

    int len = MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, str.c_str(), -1, NULL, 0);
    if(!len)
        return ret;

    wchar_t *buf = new wchar_t[len];
    if(!MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, str.c_str(), -1, buf, len))
        return ret;

    ret = buf;
    delete[] buf;

	return ret;
}

std::string utf_narrow(const std::wstring& str)
{
	std::string ret;

    int len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
    if(!len)
        return ret;

    char *buf = new char[len];
    if(!WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, buf, len, NULL, NULL))
        return ret;

    ret = buf;
    delete[] buf;

	return ret;
}