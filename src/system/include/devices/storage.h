#pragma once

class SectorDevice {
public:
	virtual int readsector(void *dst, uint index){return -1;}
};

