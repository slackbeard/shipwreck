#pragma once
#include <std/types.h>
#include <std/file.h>

class FileSystem {
public:
	virtual File *open(const char *name){return nullptr;}
};

extern FileSystem *filesystem;

