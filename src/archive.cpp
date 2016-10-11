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

#include "archive.h"
#include "util.h"
#include "filesystem.h"
#include <cstdint>
#include <cctype>
#include <cstring>
#include <cassert>

static const uint8_t KEY[] = {0x9B, 0x16, 0xFE, 0x3A, 0xB9, 0xE0, 0xA3, 0x17, 0x9A, 0x23, 0x20, 0xAE};
static const uint8_t KEY_YNK[] = {0x9B, 0x16, 0xFE, 0x3A, 0x98, 0xC2, 0xA0, 0x73, 0x0B, 0x0B, 0xB5, 0x90};

void ArchiveHeader::read(const void *data)
{
	const char *buf = (const char*)data;

	magic = read_le16(buf);
	version = read_le16(&buf[2]);
	size = read_le32(&buf[4]);
	data_offset = read_le32(&buf[8]);
	filename_table_offset = read_le32(&buf[12]);
	file_table_offset = read_le32(&buf[16]);
	dir_table_offset = read_le32(&buf[20]);
	unknown = read_le32(&buf[24]);
}

void ArchiveFilenameHeader::read(const void *data)
{
	const char *buf = (const char*)data;

	length = read_le16(buf);
	unknown = read_le16(&buf[2]);
}

void ArchiveFileHeader::read(const void *data)
{
	const char *buf = (const char*)data;

	filename_offset = read_le32(buf);
	unknown = read_le32(&buf[4]);
	unk1 = *(const uint64_t*)&buf[8];
	unk2 = *(const uint64_t*)&buf[16];
	unk3 = *(const uint64_t*)&buf[24];
	data_offset = read_le32(&buf[32]);
	data_size = read_le32(&buf[36]);
	compressed_size = read_le32(&buf[40]);
}

void ArchiveDirHeader::read(const void *data)
{
	const char *buf = (const char*)data;

	dir_offset = read_le32(buf);
	parent_dir_offset = read_le32(&buf[4]);
	num_files = read_le32(&buf[8]);
	file_header_offset = read_le32(&buf[12]);
}

bool Archive::decrypt()
{
	if(data_length_ < ARCHIVE_HEADER_SIZE)
		return false;

	if((read_le16(data_) ^ read_le16(KEY)) != ARCHIVE_MAGIC)
		return false;

    if((read_le16(&data_[2]) ^ read_le16(&KEY[2])) > 5)
        return false;

	const uint8_t *key = NULL;

	if((read_le32(&data_[8]) ^ read_le32(&KEY[8])) == 0x1c)
	{
		is_ynk_ = false;
		key = KEY;
	}
	else if((read_le32(&data_[8]) ^ read_le32(&KEY_YNK[8])) == 0x1c)
	{
		is_ynk_ = true;
		key = KEY_YNK;
	}
	else
		return false;

	std::size_t j = 0;
	for(std::size_t i = 0; i < data_length_; ++i)
	{
		data_[i] ^= key[j++];
		if(j >= sizeof(KEY))
			j = 0;
	}

	header_.read(data_);

	return true;
}

void Archive::encrypt()
{
    const uint8_t *key = is_ynk_ ? KEY_YNK : KEY;

    std::size_t j = 0;
    for(std::size_t i = 0; i < data_length_; ++i)
    {
        data_[i] ^= key[j++];
        if(j >= sizeof(KEY))
            j = 0;
    }
}

Archive::~Archive()
{
	close();
}

bool Archive::open(const std::string& filename)
{
	close();

	data_ = (uint8_t*)read_file(filename, data_length_);
	if(data_ == NULL)
        return false;

	if(!decrypt())
	{
		close();
		return false;
	}

    //write_file((filename + "_raw"), data_, data_length_);

	return true;
}

bool Archive::open(const std::wstring& filename)
{
	close();

	data_ = (uint8_t*)read_file(filename, data_length_);
	if(data_ == NULL)
        return false;

	if(!decrypt())
	{
		close();
		return false;
	}

    //write_file((filename + L"_raw"), data_, data_length_);

	return true;
}

bool Archive::save(const std::string& filename)
{
    encrypt();

    bool ret = write_file(filename, data_, data_length_);

    /* encryption is symmetrical, this is actually a decrypt */
    encrypt();

    return ret;
}

bool Archive::save(const std::wstring& filename)
{
    encrypt();

    bool ret = write_file(filename, data_, data_length_);

    /* encryption is symmetrical, this is actually a decrypt */
    encrypt();

    return ret;
}

/* WARNING: this code assumes directory headers are nested, it
* does not check for proper folder heirarchy. */
std::size_t Archive::get_header_offset(const std::string& filepath) const
{
    std::string file = filepath;
    std::size_t file_table_offset = header_.file_table_offset + header_.filename_table_offset;
    std::size_t dir_table_offset = header_.dir_table_offset + header_.filename_table_offset;
    std::size_t offset = file_table_offset;

    for(auto it = file.begin(); it != file.end(); ++it)
    {
        *it = toupper(*it);
    }

    std::size_t pos = file.find_first_of("/\\");
    std::size_t i = dir_table_offset;
    while(pos != std::string::npos)
    {
        std::string dir = file.substr(0, pos);
        bool found = false;
        for(; i < data_length_; i += ARCHIVE_DIR_HEADER_SIZE)
        {
            ArchiveDirHeader dir_header(&data_[i]);
            ArchiveFileHeader file_header(&data_[dir_header.dir_offset + file_table_offset]);
            ArchiveFilenameHeader filename_header(&data_[file_header.filename_offset + header_.filename_table_offset]);
            std::size_t name_offset = file_header.filename_offset + header_.filename_table_offset + ARCHIVE_FILENAME_HEADER_SIZE;
            std::string name;
            if(filename_header.length > 0)
                name = (const char*)&data_[name_offset];
            if(dir == name)
            {
                found = true;
                offset = file_table_offset + dir_header.file_header_offset;
                file.erase(0, pos + 1);
                break;
            }
        }
        if(!found)
            return -1;
        pos = file.find_first_of("/\\");
    }

    for(; offset < dir_table_offset; offset += ARCHIVE_FILE_HEADER_SIZE)
    {
        ArchiveFileHeader file_header(&data_[offset]);
        ArchiveFilenameHeader filename_header(&data_[file_header.filename_offset + header_.filename_table_offset]);
        std::size_t name_offset = file_header.filename_offset + header_.filename_table_offset + ARCHIVE_FILENAME_HEADER_SIZE;
        std::string name;
        if(filename_header.length > 0)
            name = (const char*)&data_[name_offset];
        if(file == name)
            return offset;
    }
    return -1;
}

std::size_t Archive::get_dir_header_offset(std::size_t file_header_offset) const
{
    std::size_t offset = header_.filename_table_offset + header_.dir_table_offset;

    for(; offset < data_length_; offset += ARCHIVE_DIR_HEADER_SIZE)
    {
        if(ArchiveDirHeader(&data_[offset]).dir_offset == file_header_offset)
            return offset;
    }

    return -1;
}

std::size_t Archive::get_file(const std::string& filepath, void *dest) const
{
    std::size_t offset = get_header_offset(filepath);

    if(offset == -1)
        return 0;

    ArchiveFileHeader file_header(&data_[offset]);

	if(dest == NULL)
		return file_header.data_size;

	uint8_t *dataptr = &data_[file_header.data_offset + header_.data_offset];

	if(file_header.compressed_size == ARCHIVE_NO_COMPRESSION)
		memcpy(dest, dataptr, file_header.data_size);
	else
		return decompress(dataptr, dest);

	return file_header.data_size;
}

std::size_t Archive::get_file(int index, void *dest) const
{
    assert(index >= 0);
    std::size_t offset = (index  * ARCHIVE_FILE_HEADER_SIZE) + header_.filename_table_offset + header_.file_table_offset;

    ArchiveFileHeader file_header(&data_[offset]);

    if(dest == NULL)
        return file_header.data_size;

    uint8_t *dataptr = &data_[file_header.data_offset + header_.data_offset];

    if(file_header.compressed_size == ARCHIVE_NO_COMPRESSION)
        memcpy(dest, dataptr, file_header.data_size);
    else
        return decompress(dataptr, dest);

    return file_header.data_size;
}

bool Archive::repack_file(const std::string & filepath, const void * src, size_t len)
{
    int index = get_index(filepath);
    if(index < 0)
        return false;
    if(filepath.empty())
        return false;

    return repack_file(index, src, len);
}

bool Archive::repack_file(int index, const void * src, size_t len)
{
    assert(index >= 0);
    size_t file_header_offset = (index  * ARCHIVE_FILE_HEADER_SIZE) + header_.filename_table_offset + header_.file_table_offset;
    if(file_header_offset >= header_.filename_table_offset + header_.dir_table_offset)
        return false;

    ArchiveFileHeader file_header(&data_[file_header_offset]);
    write_le32(&data_[file_header_offset + 36], len);
    write_le32(&data_[file_header_offset + 40], ARCHIVE_NO_COMPRESSION);

    size_t orig_len = (file_header.compressed_size == ARCHIVE_NO_COMPRESSION) ? file_header.data_size : file_header.compressed_size;
    size_t diff = len - orig_len;

    uint8_t *buf = new uint8_t[data_length_ + diff];
    memcpy(buf, data_, file_header.data_offset + header_.data_offset);
    memcpy(&buf[file_header.data_offset + header_.data_offset], src, len);

    size_t i = file_header.data_offset + header_.data_offset + len;
    size_t j = file_header.data_offset + header_.data_offset + orig_len;
    memcpy(&buf[i], &data_[j], data_length_ - j);

    data_length_ += diff;
    delete[] data_;
    data_ = buf;

    header_.filename_table_offset += diff;
    write_le32(&data_[12], header_.filename_table_offset);

    size_t file_table_offset = header_.filename_table_offset + header_.file_table_offset;
    size_t dir_table_offset = header_.filename_table_offset + header_.dir_table_offset;

    /* adjust the offsets of all files after the repacked file */
    for(size_t i = file_table_offset; i < dir_table_offset; i += ARCHIVE_FILE_HEADER_SIZE)
    {
        uint32_t *off = (uint32_t*)&data_[i + 32];
        if(*off > file_header.data_offset)
            *off += (uint32_t)diff;
    }

    return true;
}

std::string Archive::get_filename(int index) const
{
    assert(index >= 0);
    std::size_t offset = (index  * ARCHIVE_FILE_HEADER_SIZE) + header_.filename_table_offset + header_.file_table_offset;

    ArchiveFileHeader file_header(&data_[offset]);

    std::size_t name_offset = file_header.filename_offset + header_.filename_table_offset;
    ArchiveFilenameHeader name_header(&data_[name_offset]);
    std::string name;
    if(name_header.length > 0)
        name = (const char*)&data_[name_offset + ARCHIVE_FILENAME_HEADER_SIZE];
    return name;
}

std::string Archive::get_path(int index) const
{
    assert(index >= 0);
    std::size_t file_header_offset = (index  * ARCHIVE_FILE_HEADER_SIZE) + header_.filename_table_offset + header_.file_table_offset;
    std::size_t offset = header_.filename_table_offset + header_.dir_table_offset;

    for(; offset < data_length_; offset += ARCHIVE_DIR_HEADER_SIZE)
    {
        ArchiveDirHeader dir_header(&data_[offset]);
        int begin = (int)(dir_header.file_header_offset / ARCHIVE_FILE_HEADER_SIZE);
        int end = begin + dir_header.num_files;
        if((begin <= index) && (end > index))
        {
            std::string name;
            while(dir_header.parent_dir_offset != 0xFFFFFFFF)
            {
                name = get_filename(dir_header.dir_offset / ARCHIVE_FILE_HEADER_SIZE) + "/" + name;
                dir_header.read(&data_[header_.filename_table_offset + header_.dir_table_offset + dir_header.parent_dir_offset]);
            }

            return name;
        }
    }

    return std::string();
}

int Archive::get_index(const std::string& filepath) const
{
    std::size_t index = get_header_offset(filepath);

    if(index == -1)
        return -1;

    index -= (header_.filename_table_offset + header_.file_table_offset);
    index /= ARCHIVE_FILE_HEADER_SIZE;

    return index;
}

int Archive::dir_begin(int index) const
{
    assert(index >= 0);
    std::size_t offset = get_dir_header_offset(index * ARCHIVE_FILE_HEADER_SIZE);

    if(offset == -1)
        return -1;

    ArchiveDirHeader header(&data_[offset]);
    return (header.file_header_offset / ARCHIVE_FILE_HEADER_SIZE);
}

int Archive::dir_end(int index) const
{
    assert(index >= 0);
    std::size_t offset = get_dir_header_offset(index * ARCHIVE_FILE_HEADER_SIZE);

    if(offset == -1)
        return -1;

    ArchiveDirHeader header(&data_[offset]);
    int ret = (header.file_header_offset / ARCHIVE_FILE_HEADER_SIZE);
    return ret + header.num_files;
}

bool Archive::is_dir(int index) const
{
    assert(index >= 0);
    return (get_dir_header_offset(index * ARCHIVE_FILE_HEADER_SIZE) != -1);
}

void Archive::close()
{
	if(data_ != NULL)
	{
		delete[] data_;
		data_ = NULL;
	}
}

std::size_t Archive::decompress(const void *src, void *dest) const
{
	uint32_t output_size, input_size, offset, bytes_written = 0, len;
	uint8_t *outptr, key;
	const uint8_t *inptr;
	const uint8_t *endin;

	inptr = (const uint8_t*)src;
	outptr = (uint8_t*)dest;

	output_size = *((uint32_t*)&inptr[0]);
	input_size = *((uint32_t*)&inptr[4]) - 9;

	if(dest == NULL)
		return output_size;

	key = inptr[8];

	inptr += 9;
	endin = inptr + input_size;

	while(inptr < endin)
	{
		if(bytes_written >= output_size)
			return bytes_written;

		if(inptr[0] != key)
		{
			*(outptr++) = *(inptr++);
			++bytes_written;
			continue;
		}

		if(inptr[1] == key)	/* escape sequence */
		{
			*(outptr++) = key;
			inptr += 2;
			++bytes_written;
			continue;
		}

		unsigned int val = inptr[1];
		if(val > key)
			--val;

		inptr += 2;

		unsigned int offset_len = val & 3;
		len = val >> 3;

		if(val & 4)
		{
			len |= *inptr++ << 5;
		}

		len += 4;

		if(offset_len == 0)
		{
			offset = *inptr++;
		}
		else if(offset_len == 1)
		{
			offset = inptr[0];
			offset |= uint32_t(inptr[1]) << 8;
			inptr += 2;
		}
		else
		{
			offset = inptr[0];
			offset |= uint32_t(inptr[1]) << 8;
			offset |= uint32_t(inptr[2]) << 16;
			inptr += 3;
		}
		++offset;

		while(len > offset)
		{
			if(bytes_written + offset > output_size)
				return bytes_written;
			memcpy(outptr, outptr - offset, offset);
			outptr += offset;
			len -= offset;
			bytes_written += offset;
			offset += offset;
		}

		if(len > 0)
		{
			if(bytes_written + len > output_size)
				return bytes_written;
			memcpy(outptr, outptr - offset, len);
			outptr += len;
			bytes_written += len;
		}
	}

	return bytes_written;
}
