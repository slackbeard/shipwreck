#pragma once

#include <std/types.h>
#include <devices/storage.h>
#include <devices/io.h>

#define ATA_BSY  0x80
#define ATA_RDY  0x40
#define ATA_DRQ  0x08

#define ATA_DATA_PORT         0x1F0
#define ATA_ERROR_PORT        0x1F1
#define ATA_SECTOR_COUNT_PORT 0x1F2
#define ATA_LBA0_PORT         0x1F3
#define ATA_LBA1_PORT         0x1F4
#define ATA_LBA2_PORT         0x1F5
#define ATA_DRIVE_PORT        0x1F6

#define ATA_STATUS_PORT       0x1F7
#define ATA_COMMAND_PORT      0x1F7

#define ATA_ALT_STATUS_PORT   0x3F6
#define ATA_CONTROL_PORT      0x3F6

class ATADevice: public SectorDevice {
public:
	uchar get_status_byte() {
		return inb(ATA_STATUS_PORT);
	}

	uchar get_error_byte() {
		return inb(ATA_ERROR_PORT);
	}

	bool is_busy() {
		uchar status = get_status_byte();
		return (status & ATA_BSY);
	}

	bool is_data_ready() {
		uchar status = get_status_byte();
		return (status & ATA_DRQ);
	}
		
	void delay() {
		for (uint i = 0; i < 0x100; i++) {
			inb(ATA_ALT_STATUS_PORT);
		}
	}
		
	bool wait_while_busy() {
		//artificial delay
		delay();
		
		//check for BSY flag in status register
		for (uint i = 0; i < 0x100000; i++) {
			if (!is_busy()) 
				return true;
		}
		return false;
	}

	bool wait_until_data_ready() {
		//artificial delay
		delay();
		
		//check for DRQ flag in status register:
		for (uint i = 0; i < 0x100000; i++) {
			if (is_data_ready()) 
				return true;
		}
		return false;
	}

	//1 = success, 0 = fail
	int send_command(uchar command) {
		if (!wait_while_busy()) {
			//TODO: error out here
			return 0;
		}
		outb(ATA_COMMAND_PORT, command);
		return 1;
	}

	int readsector(void *dst, uint lba28) {

		//enable nIEN to disable interrupts:
		outb(ATA_CONTROL_PORT, 0x02);

		//clear device control register:
		outb(ATA_CONTROL_PORT, 0x0);
		
		//select primary drive and send high 4 bits of LBA address:
		outb(ATA_DRIVE_PORT, (uchar) (0xE0 | ((lba28 >> 24) & 0x0F)));
		if (!wait_while_busy()) return 0;

		//select sector by LBA address
		outb(ATA_SECTOR_COUNT_PORT, 1);
		outb(ATA_LBA0_PORT, (uchar) lba28);	
		outb(ATA_LBA1_PORT, (uchar) (lba28 >> 8));	
		outb(ATA_LBA2_PORT, (uchar) (lba28 >> 16));	
		
		//send PIO read command:
		if (!send_command(0x20)) return 0;

		if (!wait_until_data_ready()) return 0;

		for (int i = 0; i < 256; i++) {
			((ushort *)dst)[i] = inw(0x1F0);
		}
		return 1;
	}
};

