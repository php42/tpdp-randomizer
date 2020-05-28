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

#include "filesystem.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <filesystem>
#include <fstream>
#endif // _WIN32


bool copy_file(const std::string& src, const std::string& dest)
{
#ifdef _WIN32
    return (CopyFileA(src.c_str(), dest.c_str(), FALSE) != 0);
#else
    std::error_code ec;
    std::filesystem::copy_file(src, dest, ec);
    return !ec;
#endif // _WIN32
}

bool copy_file(const std::wstring& src, const std::wstring& dest)
{
#ifdef _WIN32
    return (CopyFileW(src.c_str(), dest.c_str(), FALSE) != 0);
#else
    std::error_code ec;
    std::filesystem::copy_file(src, dest, ec);
    return !ec;
#endif // _WIN32
}

FileBuf read_file(const std::wstring& file, std::size_t& size)
{
#ifdef _WIN32
    HANDLE infile;

    infile = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(infile == INVALID_HANDLE_VALUE)
        return FileBuf();

    LARGE_INTEGER len;
    DWORD bytes_read;
    if(!GetFileSizeEx(infile, &len))
    {
        CloseHandle(infile);
        return FileBuf();
    }

    FileBuf buf(new char[(unsigned int)len.QuadPart]);

    if(!ReadFile(infile, buf.get(), (DWORD)len.QuadPart, &bytes_read, NULL))
    {
        CloseHandle(infile);
        return FileBuf();
    }

    CloseHandle(infile);

    if(bytes_read != (DWORD)len.QuadPart)
    {
        return FileBuf();
    }

    size = (std::size_t)len.QuadPart;
    return buf;
#else
    std::filesystem::path path(file);
    std::ifstream infile(path.native(), std::ios::binary | std::ios::ate);
    if(!infile)
        return FileBuf();

    std::size_t len = infile.tellg();

    if(len == 0)
        return FileBuf();

    infile.seekg(0);

    FileBuf buf(new char[len]);

    if(!infile.read(buf.get(), len))
    {
        return FileBuf();
    }

    size = len;
    return buf;
#endif // _WIN32
}

FileBuf read_file(const std::string& file, std::size_t& size)
{
#ifdef _WIN32
    HANDLE infile;

    infile = CreateFileA(file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(infile == INVALID_HANDLE_VALUE)
        return FileBuf();

    LARGE_INTEGER len;
    DWORD bytes_read;
    if(!GetFileSizeEx(infile, &len))
    {
        CloseHandle(infile);
        return FileBuf();
    }

    FileBuf buf(new char[(unsigned int)len.QuadPart]);

    if(!ReadFile(infile, buf.get(), (DWORD)len.QuadPart, &bytes_read, NULL))
    {
        CloseHandle(infile);
        return FileBuf();
    }

    CloseHandle(infile);

    if(bytes_read != (DWORD)len.QuadPart)
    {
        return FileBuf();
    }

    size = (std::size_t)len.QuadPart;
    return buf;
#else
    std::ifstream infile(file, std::ios::binary | std::ios::ate);
    if(!infile)
        return FileBuf();

    std::size_t len = infile.tellg();

    if(len == 0)
        return FileBuf();

    infile.seekg(0);

    FileBuf buf(new char[len]);

    if(!infile.read(buf.get(), len))
    {
        return FileBuf();
    }

    size = len;
    return buf;
#endif // _WIN32
}

bool create_directory(const std::string& dir)
{
#ifdef _WIN32
    return ((CreateDirectoryA(dir.c_str(), NULL) != 0) || (GetLastError() == ERROR_ALREADY_EXISTS));
#else
    std::error_code ec;
    return std::filesystem::create_directory(dir, ec);
#endif // _WIN32
}

bool create_directory(const std::wstring& dir)
{
#ifdef _WIN32
    return ((CreateDirectoryW(dir.c_str(), NULL) != 0) || (GetLastError() == ERROR_ALREADY_EXISTS));
#else
    std::error_code ec;
    return std::filesystem::create_directory(dir, ec);
#endif // _WIN32
}

bool write_file(const std::string& file, const void *buf, std::size_t len)
{
#ifdef _WIN32
    HANDLE outfile = CreateFileA(file.c_str(), GENERIC_WRITE, /*FILE_SHARE_READ*/ 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(outfile == INVALID_HANDLE_VALUE)
        return false;

    DWORD bytes_written = 0;
    bool ret = (WriteFile(outfile, buf, (DWORD)len, &bytes_written, NULL) != 0);
    ret = (ret && (bytes_written == (DWORD)len));
    CloseHandle(outfile);
    return ret;
#else
    std::ofstream outfile(file, std::ios::binary);
    if(!outfile)
        return false;

    if(!outfile.write((const char*)buf, len))
        return false;
    return true;
#endif // _WIN32
}

bool write_file(const std::wstring& file, const void *buf, std::size_t len)
{
#ifdef _WIN32
    HANDLE outfile = CreateFileW(file.c_str(), GENERIC_WRITE, /*FILE_SHARE_READ*/ 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(outfile == INVALID_HANDLE_VALUE)
        return false;

    DWORD bytes_written = 0;
    bool ret = (WriteFile(outfile, buf, (DWORD)len, &bytes_written, NULL) != 0);
    ret = (ret && (bytes_written == (DWORD)len));
    CloseHandle(outfile);
    return ret;
#else
    std::filesystem::path path(file);
    std::ofstream outfile(path.native(), std::ios::binary);
    if(!outfile)
        return false;

    if(!outfile.write((const char*)buf, len))
        return false;
    return true;
#endif // _WIN32
}

bool path_exists(const std::string& path)
{
#ifdef _WIN32
    return (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES);
#else
    std::error_code ec;
    return (std::filesystem::exists(path, ec) && !ec);
#endif
}

bool path_exists(const std::wstring& path)
{
#ifdef _WIN32
    return (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES);
#else
    std::error_code ec;
    return (std::filesystem::exists(path, ec) && !ec);
#endif
}
