#pragma once

#include <new>
#include <std/types.h>
#include <std/string.h>
#include <devices/storage.h>
#include <util/debug.h>
#include <memory.h>
#include <filesystem.h>

#define BYTES_PER_SECTOR 512

class TarHeader {
public:
	union {
		char data[512];
		struct {
			char name[100];
			char mode[8];
			char owner[8];
			char group[8];
			char size[12];
			char mtime[12];
			char cksum[8];
			//ustar format:
			char type[1];
			char lname[100];
			char ustar[6];
			char ustar_version[2];
			char owner_name[32];
			char group_name[32];
			char dev_major[8];
			char dev_minor[8];
			char prefix[155];
		};	
	};
	uint parse_octal(char *str, uint len) {
		uint result = 0;
		for (int i = 0; (i < len) && (str[i] >= '0') && (str[i] <= '9'); i++) {
			result <<= 3;
			result += str[i] - '0';
		}
		return result;
	}

	uint get_size() {
		return parse_octal(this->size, sizeof(this->size));
	}

};


class TarFile: public File {
public:
	SectorDevice *sd;
	uint header_sector;
	uint current_sector;
	uint cursor;
	uint size;

	void init(SectorDevice *sectorDevice) {
		sd = sectorDevice;
	}
	void dump() {
		debug(9, "TarFile(sd@", (hex) sd, ",header_s=", (hex) header_sector, ",current_s=", (hex) current_sector, ",cursor=", (hex) cursor, ")");
	}

	virtual int seek(int offset) {
		if ((offset < 0) || (offset >= size)) return 0;

		cursor = offset;
		current_sector = header_sector + 1 + (offset / BYTES_PER_SECTOR);
	
		return 1;
	}	
	virtual int read(char *dst, uint length) {
		void *dstptr = (void *) dst;
		int len = length;
		if ((cursor + len) > size) {
			debug(0, "Read past EOF");
			return 0;
		}
	
		debug(9, "Read file: dst = ", (hex) dst, ", size = ", size, ", length = ", len);
		debug(9, "Sector start: ", current_sector);

		//If we're not currently aligned on a 512-byte boundary:
		int cursor_mod_512 = cursor & 0x1FF;
		if (cursor_mod_512) {
			//read all the bytes up to the next 512 boundary separately:
			char tmp[BYTES_PER_SECTOR];

			int end = cursor_mod_512 + len;
			int max = (end > BYTES_PER_SECTOR)? BYTES_PER_SECTOR: end;
			int bytes_to_read = max - cursor_mod_512;
			debug(9, "Start not aligned. max = ", max, ", bytes to read: ", bytes_to_read);
			debug(9, "Calling readsector(", (hex) &tmp, ", ", (hex) current_sector);

			sd->readsector( &tmp, current_sector);
			debug(9, "Calling memcpy(dst=", (hex) dst, ", ", (hex) (&tmp[cursor_mod_512]), ", bytes_to_read=", bytes_to_read);

			memcpy(dstptr, &tmp[cursor_mod_512], bytes_to_read);

			current_sector++;
			len -= bytes_to_read;
			dstptr = (void *)((int) dstptr + bytes_to_read);
		}

		uint whole_sects = len / BYTES_PER_SECTOR;

		debug(9, "Num of sectors: ", whole_sects);
		// Tar format is simple: all sectors are consecutive.
		for (int i = 0; i < whole_sects; i++) {
			sd->readsector(dstptr, current_sector);

			//advance sector
			current_sector++;
			dstptr = (void *) ((int) dstptr + BYTES_PER_SECTOR);
		}
		
		//if the end is not aligned:
		int end_mod_512 = len & 0x1FF;

		if (end_mod_512) {
			char tmp[BYTES_PER_SECTOR];
			debug(9, "Reading last sector with readsector(): sector = ", (hex) current_sector, ", tmp @ ", (hex) &tmp);
			sd->readsector((char *) tmp, current_sector);
			debug(9, "Calling memcopy for last sector, dst=", (hex) dstptr, ",tmp=", (hex) tmp, ", end_mod_512=", (hex) end_mod_512);

			memcpy(dstptr, &tmp, end_mod_512);
		}

		cursor += length;
debug(9, "EXITING FileSystem::fread()");
		return length;
	}
};

struct TarFS: public FileSystem {
// TODO file functions are not at all thread safe

	TarFile *files;
	uint max_files;

	uint root_sector;
	SectorDevice *sd;

	TarFS(SectorDevice *source) {
		max_files = 0x10;
		sd = source;
		root_sector = 1; // LBA sector 0 is the boot sector, 1 is the start of Tar header

		uint tf_size = (sizeof(TarFile) * max_files);
		files = (TarFile *) static_alloc_pages((tf_size + 4095) / 4096);
		for (int f = 0; f < max_files; f++) {
			new (&files[f]) TarFile;
			files[f].init(sd);
		}

	}

	TarFile *alloc_file(){
		for (int i = 0; i < max_files; i++) {
			if (files[i].size == 0) return &files[i];
		}
		return nullptr;
	}	
	void free_file(TarFile *file) {
		file->init(sd);
	}

	//Find the starting sector for a file by name, also fills in the header
	// returns 0 if file not found.
	uint find_file_sector(TarHeader &tar_header, const char *name) {
		uint name_len = strnlen(name, 0x100);

		debug(5, "Searching for file: ", name);

		uint current_sector = root_sector;

		while (1) {
			//read in the tar header:
			sd->readsector((void *) &tar_header, current_sector);

			//end of disk is marked by nulls:
			if (!tar_header.name[0]) break;

			debug(5, "   ", tar_header.get_size(), " ... ", tar_header.name, ", current_sector: ", current_sector);

			if (strncmp(tar_header.name, name, name_len) == 0) {
				debug(5, "MATCH!!! current_sector = " , current_sector);
				return current_sector;
			}
		
			//increment by the header size (1 sector) + sectors for file data
			current_sector += 1 +  (tar_header.get_size() + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR;
		}

		//not found:
		return 0;
	}

	//open file by name, mode is ignored
	File *open(const char *name) {
		TarFile *file = alloc_file();
		debug(6, "Alloc'd file @ ", (hex) file);

		if (file == nullptr) return nullptr;

		//Pass this to find_file_sector
		TarHeader header;

		if (! (file->header_sector = find_file_sector(header, name))) {
			debug(0, "File not found: ", name);

			free_file(file);
			return nullptr;
		}

		file->size = header.get_size();

		//reset file cursor
		file->seek(0);

		debug(5, "Found file '", name, "' @ ", file->current_sector);
		
		return file;
	}
};
