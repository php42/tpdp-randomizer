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

#ifndef ARCHIVE_H
#define ARCHIVE_H
#include <cstdint>
#include <string>
#include <memory>
#include <exception>

#define ARCHIVE_MAGIC 0x5844
#define ARCHIVE_HEADER_SIZE 28
#define ARCHIVE_FILENAME_HEADER_SIZE 4
#define ARCHIVE_FILE_HEADER_SIZE 44
#define ARCHIVE_DIR_HEADER_SIZE 16
#define ARCHIVE_NO_COMPRESSION 0xffffffff

struct ArcError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

class ArchiveHeader
{
public:
	uint16_t magic;					/* 0x5844 "DX" */
	uint16_t version;
	uint32_t size;					/* size of the file tables (from filename_table_offset till the end of the file) */
	uint32_t data_offset;
	uint32_t filename_table_offset;
	uint32_t file_table_offset;		/* relative to filename_table_offset */
	uint32_t dir_table_offset;		/* relative to filename_table_offset */
	uint32_t unknown;

	ArchiveHeader() {};
	ArchiveHeader(const void *data) {read(data);}

	void read(const void *data);
};

class ArchiveFilenameHeader
{
public:
	uint16_t length;				/* length of the string divided by 4 (padded with zeros if the actual string is shorter).
									 * there are actually two strings of this length, one in all caps followed by one with the real
									 * capitalization. the strings are always NULL terminated */
	uint16_t unknown;				/* checksum? */

	ArchiveFilenameHeader() {};
	ArchiveFilenameHeader(const void *data) {read(data);}

	void read(const void *data);
};

class ArchiveFileHeader
{
public:
	uint32_t filename_offset;		/* relative to filename_table_offset */
	uint32_t unknown;
	uint64_t unk1;
	uint64_t unk2;
	uint64_t unk3;
	uint32_t data_offset;			/* relative to data_offset in the archive header */
	uint32_t data_size;
	uint32_t compressed_size;		/* 0xffffffff for no compression */

	ArchiveFileHeader() {};
	ArchiveFileHeader(const void *data) {read(data);}

	void read(const void *data);
};

class ArchiveDirHeader
{
public:
	uint32_t dir_offset;			/* offset of the directory's own file header relative to file_table_offset in the archive header */
	uint32_t parent_dir_offset;		/* relative to dir_table_offset */
	uint32_t num_files;				/* number of files in the directory */
	uint32_t file_header_offset;	/* offset of the directory's contents file headers relative to file_table_offset */

	ArchiveDirHeader() {};
	ArchiveDirHeader(const void *data) {read(data);}

	void read(const void *data);
};

/* container for files extracted from an Archive */
class ArcFile
{
private:
    std::unique_ptr<char[]> buf_;
    std::size_t len_;
    int index_;

public:
    ArcFile() : buf_(), len_(0), index_(-1) {};
    ArcFile(char *buf, std::size_t len, int index) : buf_(buf), len_(len), index_(index) {};
    ArcFile(const ArcFile&) = delete;
    ArcFile(ArcFile&& other) : buf_(std::move(other.buf_)), len_(other.len_), index_(other.index_) { other.reset(); };

    ArcFile& operator =(const ArcFile&) = delete;
    ArcFile& operator =(ArcFile&& other) { buf_ = std::move(other.buf_); len_ = other.len_; index_ = other.index_; other.reset(); return *this; }

    char *data() const { return buf_.get(); }
    std::size_t size() const { return len_; }
    int file_index() const { return index_; }

    void reset() { buf_.reset(); len_ = 0; index_ = -1; }
    void reset(char *buf, std::size_t sz, int index) { buf_.reset(buf); len_ = sz; index_ = index; }

    explicit operator bool() const { return ((bool)buf_ && len_ && (index_ > 0)); }
};

class Archive
{
private:
	ArchiveHeader header_;
	std::size_t data_used_, data_max_;
    std::unique_ptr<char[]> data_;
	bool is_ynk_;

	Archive(const Archive&) = delete;
	Archive& operator=(const Archive&) = delete;

    std::size_t get_header_offset(const std::string& filepath) const;
    std::size_t get_dir_header_offset(std::size_t file_header_offset) const;

	void parse();
    void encrypt();
    void decrypt() { encrypt(); } // encryption is symmetical, this is an alias of encrypt()

	std::size_t decompress(const void *src, void *dest) const;

public:
	Archive() : header_(), data_used_(0), data_max_(0), is_ynk_(false) {};
    ~Archive() { close(); }

    /* allow move semantics */
    Archive(Archive&&) = default;
    Archive& operator=(Archive&&) = default;

    explicit operator bool() const { return (data_ != nullptr); }

    /* throws ArcError on failure */
	void open(const std::string& filename);
	void open(const std::wstring& filename);

    bool save(const std::string& filename);
    bool save(const std::wstring& filename);

	/* returns 0 on error, nonzero otherwise. if dest is NULL, returns decompressed size of the requested file */
	std::size_t get_file(const std::string& filepath, void *dest) const;
    std::size_t get_file(int index, void *dest) const;

    /* returns empty ArcFile on error */
    ArcFile get_file(const std::string& filepath) const;
    ArcFile get_file(int index) const;

    /* this will replace existing files only */
    bool repack_file(const std::string& filepath, const void *src, size_t len);
    bool repack_file(int index, const void *src, size_t len);
    bool repack_file(const ArcFile& file);

    std::string get_filename(int index) const;
    std::string get_path(int index) const;

    /* returns -1 on error */
    int get_index(const std::string& filepath) const;
    int dir_begin(int index) const;
    int dir_end(int index) const;

    bool is_dir(int index) const;

	bool is_ynk() const {return is_ynk_;}

    void close() { data_.reset(); data_used_ = 0; data_max_ = 0; is_ynk_ = false; }
};

#endif // ARCHIVE_H
